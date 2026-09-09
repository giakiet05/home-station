package collector

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"log/slog"
	"strings"
	"sync"
	"time"

	"github.com/giakiet05/home-station/server/internal/config"
	"github.com/giakiet05/home-station/server/internal/model"
	"go.bug.st/serial"
)

// TelemetryListener defines a callback invoked on fresh telemetry or state changes.
type TelemetryListener func(t model.Telemetry)

// SerialCollector handles background serial data ingestion and concurrency-safe storage.
type SerialCollector struct {
	cfg        *config.Config
	logger     *slog.Logger
	mu         sync.RWMutex
	latest     model.Telemetry
	reconnects uint64
	listener   TelemetryListener
}

// NewSerialCollector initializes a new SerialCollector instance.
func NewSerialCollector(cfg *config.Config, logger *slog.Logger) *SerialCollector {
	return &SerialCollector{
		cfg:    cfg,
		logger: logger,
		latest: model.Telemetry{
			DeviceOnline: false,
			SmokeStatus:  "WAITING FOR DATA",
			LastSeen:     time.Time{},
		},
	}
}

// SetListener registers a callback invoked whenever telemetry arrives or device state transitions.
func (c *SerialCollector) SetListener(listener TelemetryListener) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.listener = listener
}

// GetLatest returns a thread-safe copy of the latest recorded sensor telemetry.
func (c *SerialCollector) GetLatest() model.Telemetry {
	c.mu.RLock()
	defer c.mu.RUnlock()

	copyTelemetry := c.latest
	// Consider device offline if no new telemetry arrived in last 60 seconds
	if !copyTelemetry.LastSeen.IsZero() && time.Since(copyTelemetry.LastSeen) > 60*time.Second {
		copyTelemetry.DeviceOnline = false
	}
	return copyTelemetry
}

// ParseLine parses a single raw line string into a valid Telemetry object.
func (c *SerialCollector) ParseLine(line string) (*model.Telemetry, error) {
	line = strings.TrimSpace(line)
	if line == "" || !strings.HasPrefix(line, "{") || !strings.HasSuffix(line, "}") {
		return nil, fmt.Errorf("line is not valid JSON payload: %s", line)
	}

	var raw model.RawESP32Payload
	if err := json.Unmarshal([]byte(line), &raw); err != nil {
		return nil, fmt.Errorf("failed to unmarshal JSON payload: %w", err)
	}

	telemetry := &model.Telemetry{
		Temperature:  raw.Temperature,
		Humidity:     raw.Humidity,
		SmokeRawADC:  raw.SmokeRawADC,
		SmokeVoltage: raw.SmokeVoltage,
		SmokePercent: raw.SmokePercent,
		SmokeStatus:  raw.Status,
		LightRawADC:  raw.LightRawADC,
		LightPercent: raw.LightPercent,
		LightStatus:  raw.LightStatus,
		IsNightMode:  raw.IsNightMode,
		UptimeSec:    raw.UptimeMs / 1000,
		DeviceOnline: true,
		LastSeen:     time.Now().UTC(),
	}

	return telemetry, nil
}

// Start initiates the collector loop in the background.
func (c *SerialCollector) Start(ctx context.Context) {
	// Start liveness watchdog
	go c.watchdogLoop(ctx)

	if c.cfg.MockMode {
		c.logger.Info("Serial collector starting in MOCK mode")
		go c.runMockLoop(ctx)
		return
	}

	c.logger.Info("Serial collector starting", "port", c.cfg.SerialPort, "baud", c.cfg.BaudRate)
	go c.runSerialLoop(ctx)
}

// watchdogLoop periodically checks telemetry freshness and notifies listeners.
func (c *SerialCollector) watchdogLoop(ctx context.Context) {
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			current := c.GetLatest()
			c.mu.RLock()
			listener := c.listener
			c.mu.RUnlock()

			if listener != nil {
				listener(current)
			}
		}
	}
}

// runSerialLoop continuously reads serial data and handles automatic reconnection.
func (c *SerialCollector) runSerialLoop(ctx context.Context) {
	mode := &serial.Mode{
		BaudRate: c.cfg.BaudRate,
		DataBits: 8,
		Parity:   serial.NoParity,
		StopBits: serial.OneStopBit,
	}

	for {
		select {
		case <-ctx.Done():
			c.logger.Info("Serial collector loop stopped by context")
			return
		default:
		}

		port, err := serial.Open(c.cfg.SerialPort, mode)
		if err != nil {
			c.logger.Warn("Failed to open serial port, retrying in 2 seconds",
				"port", c.cfg.SerialPort,
				"error", err.Error())

			c.mu.Lock()
			c.latest.DeviceOnline = false
			listener := c.listener
			c.mu.Unlock()

			if listener != nil {
				listener(c.GetLatest())
			}

			select {
			case <-ctx.Done():
				return
			case <-time.After(2 * time.Second):
				continue
			}
		}

		c.logger.Info("Serial port opened successfully", "port", c.cfg.SerialPort)

		heartbeatCtx, cancelHeartbeat := context.WithCancel(ctx)
		go c.writeHeartbeatLoop(heartbeatCtx, port)

		c.readFromPort(ctx, port)
		cancelHeartbeat()
		_ = port.Close()

		c.mu.Lock()
		c.latest.DeviceOnline = false
		c.reconnects++
		listener := c.listener
		c.mu.Unlock()

		if listener != nil {
			listener(c.GetLatest())
		}

		select {
		case <-ctx.Done():
			return
		case <-time.After(1 * time.Second):
		}
	}
}

// readFromPort consumes lines from the open serial stream.
func (c *SerialCollector) readFromPort(ctx context.Context, port serial.Port) {
	scanner := bufio.NewScanner(port)

	for scanner.Scan() {
		select {
		case <-ctx.Done():
			return
		default:
		}

		text := scanner.Text()
		telemetry, err := c.ParseLine(text)
		if err != nil {
			// Skip non-JSON boot/debug lines silently
			c.logger.Debug("Non-telemetry serial line received", "line", text)
			continue
		}

		c.mu.Lock()
		c.latest = *telemetry
		listener := c.listener
		c.mu.Unlock()

		if listener != nil {
			listener(*telemetry)
		}

		c.logger.Debug("Telemetry updated",
			"temp", telemetry.Temperature,
			"humidity", telemetry.Humidity,
			"smoke_adc", telemetry.SmokeRawADC,
			"status", telemetry.SmokeStatus)
	}

	if err := scanner.Err(); err != nil {
		c.logger.Error("Error scanning serial port stream", "error", err.Error())
	}
}

// writeHeartbeatLoop periodically writes ping frames down the serial wire to maintain watchdog heartbeat.
func (c *SerialCollector) writeHeartbeatLoop(ctx context.Context, port serial.Port) {
	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			payload := []byte("{\"type\":\"ping\"}\n")
			if _, err := port.Write(payload); err != nil {
				c.logger.Debug("Failed to write serial heartbeat frame", "error", err.Error())
				return
			}
		}
	}
}

// runMockLoop generates periodic mock telemetry for testing without physical hardware.
func (c *SerialCollector) runMockLoop(ctx context.Context) {
	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()

	var counter uint64
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			counter += 2
			t := model.Telemetry{
				Temperature:  31.5,
				Humidity:     75.0,
				SmokeRawADC:  380,
				SmokeVoltage: 0.306,
				SmokePercent: 9.3,
				SmokeStatus:  "NORMAL",
				LightRawADC:  1850,
				LightPercent: 45.2,
				LightStatus:  "INDOOR LIGHT",
				IsNightMode:  false,
				UptimeSec:    counter,
				DeviceOnline: true,
				LastSeen:     time.Now().UTC(),
			}

			c.mu.Lock()
			c.latest = t
			listener := c.listener
			c.mu.Unlock()

			if listener != nil {
				listener(t)
			}
		}
	}
}
