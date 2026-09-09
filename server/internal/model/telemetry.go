package model

import "time"

// RawESP32Payload defines the incoming JSON structure sent over serial from ESP32.
type RawESP32Payload struct {
	Temperature     float64 `json:"temp"`
	Humidity        float64 `json:"humidity"`
	SmokeRawADC     uint16  `json:"smoke_raw"`
	SmokeVoltage    float64 `json:"smoke_voltage"`
	SmokePercent    float64 `json:"smoke_pct"`
	Status          string  `json:"status"`
	LightRawADC     uint16  `json:"light_raw"`
	LightPercent    float64 `json:"light_pct"`
	LightStatus     string  `json:"light_status"`
	IsNightMode     bool    `json:"is_night"`
	Presence        bool    `json:"presence"`
	BLERssi         int8    `json:"ble_rssi"`
	PresenceTrigger string  `json:"presence_trigger"`
	UptimeMs        uint64  `json:"uptime_ms"`
}

// Telemetry represents the processed and timestamped environment sensor snapshot.
type Telemetry struct {
	Temperature      float64   `json:"temperature"`
	Humidity         float64   `json:"humidity"`
	SmokeRawADC      uint16    `json:"smoke_raw_adc"`
	SmokeVoltage     float64   `json:"smoke_voltage"`
	SmokePercent     float64   `json:"smoke_percent"`
	SmokeStatus      string    `json:"smoke_status"`
	LightRawADC      uint16    `json:"light_raw_adc"`
	LightPercent     float64   `json:"light_percent"`
	LightStatus      string    `json:"light_status"`
	IsNightMode      bool      `json:"is_night_mode"`
	PresenceDetected bool      `json:"presence_detected"`
	BLERssi          int8      `json:"ble_rssi"`
	PresenceTrigger  string    `json:"presence_trigger"`
	UptimeSec        uint64    `json:"uptime_seconds"`
	DeviceOnline     bool      `json:"device_online"`
	LastSeen         time.Time `json:"last_seen"`
}

// HomepageWidgetResponse represents formatted string values tailored for Homepage customapi widget.
type HomepageWidgetResponse struct {
	Temperature string `json:"temperature"`
	Humidity    string `json:"humidity"`
	SmokeLevel  string `json:"smoke_level"`
	LightLevel  string `json:"light_level"`
	BLESignal   string `json:"ble_signal"`
	Status      string `json:"status"`
	DeviceState string `json:"device_state"`
	LastUpdated string `json:"last_updated"`
}

