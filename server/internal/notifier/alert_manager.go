package notifier

import (
	"context"
	"fmt"
	"log/slog"
	"sync"
	"time"

	"github.com/giakiet05/home-station/server/internal/config"
	"github.com/giakiet05/home-station/server/internal/model"
)

// AlertSeverity represents the classification of environment status for alert triggering.
type AlertSeverity string

const (
	SeverityNormal   AlertSeverity = "NORMAL"
	SeverityWarning  AlertSeverity = "WARNING"
	SeverityDanger   AlertSeverity = "DANGER"
	SeverityOffline  AlertSeverity = "OFFLINE"
)

// AlertManager tracks state transitions and enforces cooldown intervals to prevent alert spam.
type AlertManager struct {
	cfg            *config.Config
	client         TelegramClient
	logger         *slog.Logger
	mu             sync.Mutex
	lastSeverity   AlertSeverity
	lastAlertTime  time.Time
	wasOffline     bool
}

// NewAlertManager creates a new AlertManager instance.
func NewAlertManager(cfg *config.Config, client TelegramClient, logger *slog.Logger) *AlertManager {
	return &AlertManager{
		cfg:          cfg,
		client:       client,
		logger:       logger,
		lastSeverity: SeverityNormal,
		wasOffline:   false,
	}
}

// ProcessTelemetry evaluates a fresh telemetry snapshot and triggers Telegram notifications if necessary.
func (am *AlertManager) ProcessTelemetry(ctx context.Context, t model.Telemetry) {
	if !am.cfg.TelegramEnabled || am.cfg.TelegramChatID == 0 {
		return
	}

	am.mu.Lock()
	defer am.mu.Unlock()

	now := time.Now().UTC()
	cooldown := time.Duration(am.cfg.AlertCooldownSec) * time.Second

	// 1. Check Device Offline condition
	if !t.DeviceOnline {
		if !am.wasOffline {
			am.wasOffline = true
			am.lastSeverity = SeverityOffline
			msg := "<b>[ALERT] Device Offline</b>\n\nHome Station ESP32 has stopped transmitting telemetry over serial.\nPlease check USB cable connection on homeserver."
			am.sendAlert(ctx, msg)
		}
		return
	}

	// If recovering from offline
	if am.wasOffline {
		am.wasOffline = false
		msg := fmt.Sprintf("<b>[RECOVERY] Device Online</b>\n\nHome Station ESP32 reconnected successfully.\nTemp: <code>%.1f°C</code> | Humidity: <code>%.1f%%</code> | Smoke ADC: <code>%d</code>",
			t.Temperature, t.Humidity, t.SmokeRawADC)
		am.sendAlert(ctx, msg)
	}

	// 2. Determine current severity
	tempThreshold := am.cfg.TempThresholdWarn
	if tempThreshold <= 0 {
		tempThreshold = 40.0
	}

	currentSeverity := SeverityNormal
	if t.SmokeStatus == "DANGER" || t.SmokeStatus == "DANGER (Smoke/Gas Detected)" {
		currentSeverity = SeverityDanger
	} else if t.SmokeStatus == "WARNING" || t.SmokeStatus == "WARNING (Elevated Gas)" || (t.Temperature >= tempThreshold) {
		currentSeverity = SeverityWarning
	}

	// 3. Handle Danger & Warning alerts
	if currentSeverity == SeverityDanger {
		if am.lastSeverity != SeverityDanger || now.Sub(am.lastAlertTime) >= cooldown {
			am.lastSeverity = SeverityDanger
			am.lastAlertTime = now

			msg := fmt.Sprintf("🚨 <b>[EMERGENCY] Smoke / Gas Danger Detected!</b>\n\n"+
				"<b>Status:</b> <code>%s</code>\n"+
				"<b>Smoke Level:</b> <code>%d ADC (%.2fV)</code>\n"+
				"<b>Temperature:</b> <code>%.1f°C</code>\n"+
				"<b>Humidity:</b> <code>%.1f%%</code>\n"+
				"<b>Time:</b> <code>%s</code>\n\n"+
				"⚠️ <i>Please inspect your room immediately!</i>",
				t.SmokeStatus, t.SmokeRawADC, t.SmokeVoltage, t.Temperature, t.Humidity, now.Format("2006-01-02 15:04:05 UTC"))

			am.sendAlert(ctx, msg)
		}
		return
	}

	if currentSeverity == SeverityWarning {
		if am.lastSeverity != SeverityWarning || now.Sub(am.lastAlertTime) >= cooldown {
			am.lastSeverity = SeverityWarning
			am.lastAlertTime = now

			msg := fmt.Sprintf("⚠️ <b>[WARNING] Elevated Environmental Hazard</b>\n\n"+
				"<b>Status:</b> <code>%s</code>\n"+
				"<b>Smoke Level:</b> <code>%d ADC (%.2fV)</code>\n"+
				"<b>Temperature:</b> <code>%.1f°C</code>\n"+
				"<b>Humidity:</b> <code>%.1f%%</code>\n"+
				"<b>Time:</b> <code>%s</code>",
				t.SmokeStatus, t.SmokeRawADC, t.SmokeVoltage, t.Temperature, t.Humidity, now.Format("2006-01-02 15:04:05 UTC"))

			am.sendAlert(ctx, msg)
		}
		return
	}

	// 4. Handle Recovery to Normal
	if currentSeverity == SeverityNormal {
		if am.lastSeverity == SeverityDanger || am.lastSeverity == SeverityWarning {
			msg := fmt.Sprintf("✅ <b>[RECOVERY] Environmental Levels Stabilized</b>\n\n"+
				"<b>Status:</b> <code>NORMAL / SAFE</code>\n"+
				"<b>Smoke Level:</b> <code>%d ADC (%.2fV)</code>\n"+
				"<b>Temperature:</b> <code>%.1f°C</code>\n"+
				"<b>Humidity:</b> <code>%.1f%%</code>\n"+
				"<b>Time:</b> <code>%s</code>",
				t.SmokeRawADC, t.SmokeVoltage, t.Temperature, t.Humidity, now.Format("2006-01-02 15:04:05 UTC"))

			am.lastSeverity = SeverityNormal
			am.lastAlertTime = now
			am.sendAlert(ctx, msg)
		}
	}
}

// sendAlert dispatches message to configured chat ID.
func (am *AlertManager) sendAlert(ctx context.Context, text string) {
	go func() {
		sendCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()

		if err := am.client.SendMessage(sendCtx, am.cfg.TelegramChatID, text); err != nil {
			am.logger.Error("Failed to send telegram notification",
				"chat_id", am.cfg.TelegramChatID,
				"error", err.Error())
		} else {
			am.logger.Info("Telegram alert dispatched successfully",
				"chat_id", am.cfg.TelegramChatID)
		}
	}()
}
