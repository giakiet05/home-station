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

// SerialCollector handles background serial data ingestion and concurrency-safe storage.
type SerialCollector struct {
	cfg        *config.Config
	logger     *slog.Logger
	mu         sync.RWMutex
	latest     model.Telemetry
	reconnects uint64
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

// GetLatest returns a thread-safe copy of the latest recorded sensor telemetry.
func (c *SerialCollector) GetLatest() model.Telemetry {
	c.mu.RLock()
	defer c.mu.RUnlock()

	copyTelemetry := c.latest
	// Consider device offline if no new telemetry arrived in last 10 seconds
	if !copyTelemetry.LastSeen.IsZero() && time.Since(copyTelemetry.LastSeen) > 10*time.Second {
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
		UptimeSec:    raw.UptimeMs / 1000,
		DeviceOnline: true,
		LastSeen:     time.Now().UTC(),
	}

	return telemetry, nil
}

// Start initiates the collector loop in the background.
func (c *SerialCollector) Start(ctx context.Context) {
	if c.cfg.MockMode {
		c.logger.Info("Serial collector starting in MOCK mode")
		go c.runMockLoop(ctx)
		return
	}

	c.logger.Info("Serial collector starting", "port", c.cfg.SerialPort, "baud", c.cfg.BaudRate)
	go c.runSerialLoop(ctx)
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
			c.mu.Unlock()

			select {
			case <-ctx.Done():
				return
			case <-time.After(2 * time.Second):
				continue
			}
		}

		c.logger.Info("Serial port opened successfully", "port", c.cfg.SerialPort)
		c.readFromPort(ctx, port)
		_ = port.Close()

		c.mu.Lock()
		c.latest.DeviceOnline = false
		c.reconnects++
		c.mu.Unlock()

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
		c.mu.Unlock()

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
				UptimeSec:    counter,
				DeviceOnline: true,
				LastSeen:     time.Now().UTC(),
			}

			c.mu.Lock()
			c.latest = t
			c.mu.Unlock()
		}
	}
}
