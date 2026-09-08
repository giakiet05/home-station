package collector

import (
	"log/slog"
	"os"
	"testing"
	"time"

	"github.com/giakiet05/home-station/server/internal/config"
)

func TestParseLine_ValidJSON(t *testing.T) {
	cfg := &config.Config{MockMode: true}
	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))
	c := NewSerialCollector(cfg, logger)

	line := `{"temp":32.8,"humidity":76.0,"smoke_raw":380,"smoke_voltage":0.306,"smoke_pct":9.3,"status":"NORMAL","uptime_ms":4526}`
	telemetry, err := c.ParseLine(line)
	if err != nil {
		t.Fatalf("expected nil error, got %v", err)
	}

	if telemetry.Temperature != 32.8 {
		t.Errorf("expected temp 32.8, got %f", telemetry.Temperature)
	}
	if telemetry.Humidity != 76.0 {
		t.Errorf("expected humidity 76.0, got %f", telemetry.Humidity)
	}
	if telemetry.SmokeRawADC != 380 {
		t.Errorf("expected smoke_raw 380, got %d", telemetry.SmokeRawADC)
	}
	if telemetry.SmokeStatus != "NORMAL" {
		t.Errorf("expected status NORMAL, got %s", telemetry.SmokeStatus)
	}
	if telemetry.UptimeSec != 4 {
		t.Errorf("expected uptime 4s, got %d", telemetry.UptimeSec)
	}
	if !telemetry.DeviceOnline {
		t.Errorf("expected DeviceOnline true, got false")
	}
}

func TestParseLine_InvalidLines(t *testing.T) {
	cfg := &config.Config{MockMode: true}
	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))
	c := NewSerialCollector(cfg, logger)

	testCases := []string{
		"",
		"ESP-ROM:esp32c3-api1-20210207",
		"[I2C] Scanning I2C bus...",
		"{invalid json",
	}

	for _, tc := range testCases {
		_, err := c.ParseLine(tc)
		if err == nil {
			t.Errorf("expected error for invalid line '%s', got nil", tc)
		}
	}
}

func TestGetLatest_OfflineTimeout(t *testing.T) {
	cfg := &config.Config{MockMode: true}
	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))
	c := NewSerialCollector(cfg, logger)

	c.mu.Lock()
	c.latest.DeviceOnline = true
	c.latest.LastSeen = time.Now().Add(-35 * time.Second) // 35 seconds ago
	c.mu.Unlock()

	latest := c.GetLatest()
	if latest.DeviceOnline {
		t.Errorf("expected DeviceOnline to be false due to 30s timeout, got true")
	}
}
