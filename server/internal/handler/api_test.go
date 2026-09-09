package handler

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/giakiet05/home-station/server/internal/model"
)

type mockProvider struct {
	telemetry model.Telemetry
}

func (m *mockProvider) GetLatest() model.Telemetry {
	return m.telemetry
}

func TestHealthCheck(t *testing.T) {
	provider := &mockProvider{}
	router := NewRouter(provider)

	req := httptest.NewRequest(http.MethodGet, "/healthz", nil)
	rec := httptest.NewRecorder()

	router.ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("expected status 200, got %d", rec.Code)
	}

	var body map[string]string
	if err := json.NewDecoder(rec.Body).Decode(&body); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}

	if body["status"] != "ok" {
		t.Errorf("expected status 'ok', got '%s'", body["status"])
	}
}

func TestGetLatestTelemetry(t *testing.T) {
	expected := model.Telemetry{
		Temperature:  32.5,
		Humidity:     76.5,
		SmokeRawADC:  385,
		SmokeVoltage: 0.310,
		SmokePercent: 9.4,
		SmokeStatus:  "NORMAL",
		UptimeSec:    120,
		DeviceOnline: true,
		LastSeen:     time.Now().UTC(),
	}

	provider := &mockProvider{telemetry: expected}
	router := NewRouter(provider)

	req := httptest.NewRequest(http.MethodGet, "/api/v1/sensors/latest", nil)
	rec := httptest.NewRecorder()

	router.ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("expected status 200, got %d", rec.Code)
	}

	var actual model.Telemetry
	if err := json.NewDecoder(rec.Body).Decode(&actual); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}

	if actual.Temperature != expected.Temperature {
		t.Errorf("expected temp %f, got %f", expected.Temperature, actual.Temperature)
	}
	if actual.SmokeRawADC != expected.SmokeRawADC {
		t.Errorf("expected smoke_raw %d, got %d", expected.SmokeRawADC, actual.SmokeRawADC)
	}
}

func TestGetHomepageWidgetData(t *testing.T) {
	provider := &mockProvider{
		telemetry: model.Telemetry{
			Temperature:  32.0,
			Humidity:     75.0,
			SmokeRawADC:  380,
			SmokeStatus:  "NORMAL",
			LightRawADC:  1850,
			LightStatus:  "INDOOR LIGHT",
			DeviceOnline: true,
			LastSeen:     time.Now().UTC(),
		},
	}
	router := NewRouter(provider)

	req := httptest.NewRequest(http.MethodGet, "/api/v1/homepage", nil)
	rec := httptest.NewRecorder()

	router.ServeHTTP(rec, req)

	if rec.Code != http.StatusOK {
		t.Fatalf("expected status 200, got %d", rec.Code)
	}

	var widget model.HomepageWidgetResponse
	if err := json.NewDecoder(rec.Body).Decode(&widget); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}

	if widget.Temperature != "32.0°C" {
		t.Errorf("expected temperature '32.0°C', got '%s'", widget.Temperature)
	}
	if widget.Humidity != "75.0%" {
		t.Errorf("expected humidity '75.0%%', got '%s'", widget.Humidity)
	}
	if widget.LightLevel != "Moderate / Indoor Light (1850)" {
		t.Errorf("expected light level 'Moderate / Indoor Light (1850)', got '%s'", widget.LightLevel)
	}
	if widget.DeviceState != "Online" {
		t.Errorf("expected device state 'Online', got '%s'", widget.DeviceState)
	}
}

