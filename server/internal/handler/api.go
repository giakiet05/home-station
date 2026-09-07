package handler

import (
	"encoding/json"
	"fmt"
	"net/http"
	"time"

	"github.com/giakiet05/home-station/server/internal/model"
)

// TelemetryProvider abstracts telemetry retrieval for HTTP handlers.
type TelemetryProvider interface {
	GetLatest() model.Telemetry
}

// Router wraps the HTTP multiplexer with API routes.
type Router struct {
	provider TelemetryProvider
	mux      *http.ServeMux
}

// NewRouter constructs a new Router with registered endpoints.
func NewRouter(provider TelemetryProvider) *Router {
	r := &Router{
		provider: provider,
		mux:      http.NewServeMux(),
	}
	r.registerRoutes()
	return r
}

// ServeHTTP implements http.Handler.
func (r *Router) ServeHTTP(w http.ResponseWriter, req *http.Request) {
	r.mux.ServeHTTP(w, req)
}

// registerRoutes configures all application HTTP endpoints.
func (r *Router) registerRoutes() {
	r.mux.HandleFunc("GET /healthz", r.handleHealthCheck)
	r.mux.HandleFunc("GET /api/v1/sensors/latest", r.handleGetLatestTelemetry)
	r.mux.HandleFunc("GET /api/v1/homepage", r.handleGetHomepageWidgetData)
	r.mux.HandleFunc("GET /", r.handleDashboardPage)
}

// handleHealthCheck returns service health status.
func (r *Router) handleHealthCheck(w http.ResponseWriter, req *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	_ = json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
}

// handleGetLatestTelemetry responds with raw structured telemetry data.
func (r *Router) handleGetLatestTelemetry(w http.ResponseWriter, req *http.Request) {
	telemetry := r.provider.GetLatest()

	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	_ = json.NewEncoder(w).Encode(telemetry)
}

// handleGetHomepageWidgetData returns string-formatted values for Homepage customapi widget.
func (r *Router) handleGetHomepageWidgetData(w http.ResponseWriter, req *http.Request) {
	t := r.provider.GetLatest()

	var tempStr string
	var humStr string
	if t.Temperature > 0 {
		tempStr = fmt.Sprintf("%.1f°C", t.Temperature)
	} else {
		tempStr = "N/A"
	}

	if t.Humidity > 0 {
		humStr = fmt.Sprintf("%.1f%%", t.Humidity)
	} else {
		humStr = "N/A"
	}

	smokeStr := fmt.Sprintf("%s (%d)", t.SmokeStatus, t.SmokeRawADC)
	deviceState := "Online"
	if !t.DeviceOnline {
		deviceState = "Offline"
	}

	lastUpdated := "Never"
	if !t.LastSeen.IsZero() {
		lastUpdated = t.LastSeen.Format(time.RFC3339)
	}

	response := model.HomepageWidgetResponse{
		Temperature: tempStr,
		Humidity:    humStr,
		SmokeLevel:  smokeStr,
		Status:      t.SmokeStatus,
		DeviceState: deviceState,
		LastUpdated: lastUpdated,
	}

	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	_ = json.NewEncoder(w).Encode(response)
}

// handleDashboardPage renders a clean, self-contained Dark Mode status card.
func (r *Router) handleDashboardPage(w http.ResponseWriter, req *http.Request) {
	if req.URL.Path != "/" {
		http.NotFound(w, req)
		return
	}

	t := r.provider.GetLatest()
	w.Header().Set("Content-Type", "text/html; charset=utf-8")

	html := fmt.Sprintf(`<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="refresh" content="3">
    <title>Home Station Monitor</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 24px; display: flex; justify-content: center; align-items: center; min-height: 90vh; }
        .card { background: #1e293b; border-radius: 16px; padding: 32px; max-width: 480px; width: 100%%; box-shadow: 0 10px 25px rgba(0,0,0,0.5); border: 1px solid #334155; }
        .header { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; padding-bottom: 16px; margin-bottom: 24px; }
        .title { font-size: 1.25rem; font-weight: 700; color: #38bdf8; }
        .badge { padding: 4px 10px; border-radius: 20px; font-size: 0.8rem; font-weight: 600; background: %s; color: #fff; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 20px; }
        .metric { background: #0f172a; padding: 16px; border-radius: 12px; border: 1px solid #334155; }
        .label { font-size: 0.8rem; color: #94a3b8; text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 4px; }
        .value { font-size: 1.6rem; font-weight: 700; color: #f8fafc; }
        .full-width { grid-column: span 2; }
        .footer { font-size: 0.75rem; color: #64748b; text-align: center; margin-top: 16px; }
    </style>
</head>
<body>
    <div class="card">
        <div class="header">
            <div class="title">Home Station</div>
            <div class="badge">%s</div>
        </div>
        <div class="grid">
            <div class="metric">
                <div class="label">Temperature</div>
                <div class="value">%.1f &deg;C</div>
            </div>
            <div class="metric">
                <div class="label">Humidity</div>
                <div class="value">%.1f %%</div>
            </div>
            <div class="metric full-width">
                <div class="label">Smoke & Gas Detection</div>
                <div class="value" style="font-size: 1.25rem;">%s <span style="font-size: 0.9rem; color: #94a3b8;">(ADC: %d)</span></div>
            </div>
        </div>
        <div class="footer">
            Auto-refreshing every 3s &bull; ESP32-C3 Super Mini &bull; Last seen: %s
        </div>
    </div>
</body>
</html>`,
		getBadgeColor(t.DeviceOnline),
		getDeviceStatusString(t.DeviceOnline),
		t.Temperature,
		t.Humidity,
		t.SmokeStatus,
		t.SmokeRawADC,
		t.LastSeen.Format("15:04:05 UTC"),
	)

	_, _ = w.Write([]byte(html))
}

func getBadgeColor(online bool) string {
	if online {
		return "#10b981" // Green
	}
	return "#ef4444" // Red
}

func getDeviceStatusString(online bool) string {
	if online {
		return "ONLINE"
	}
	return "OFFLINE"
}
