package notifier

import (
	"context"
	"fmt"
	"log/slog"
	"strings"
	"time"

	"github.com/giakiet05/home-station/server/internal/config"
	"github.com/giakiet05/home-station/server/internal/model"
)

// TelemetryGetter provides thread-safe access to current sensor telemetry.
type TelemetryGetter interface {
	GetLatest() model.Telemetry
}

// BotListener handles incoming interactive commands from authorized Telegram users.
type BotListener struct {
	cfg       *config.Config
	client    TelegramClient
	telemetry TelemetryGetter
	logger    *slog.Logger
}

// NewBotListener initializes an interactive Telegram bot listener.
func NewBotListener(cfg *config.Config, client TelegramClient, telemetry TelemetryGetter, logger *slog.Logger) *BotListener {
	return &BotListener{
		cfg:       cfg,
		client:    client,
		telemetry: telemetry,
		logger:    logger,
	}
}

// Start begins the long polling update loop in the background.
func (b *BotListener) Start(ctx context.Context) {
	if !b.cfg.TelegramEnabled || b.cfg.TelegramBotToken == "" || b.cfg.TelegramChatID == 0 {
		b.logger.Info("Interactive Telegram bot listener is disabled (missing token or chat ID)")
		return
	}

	b.logger.Info("Starting interactive Telegram bot listener",
		"authorized_chat_id", b.cfg.TelegramChatID)

	go b.pollLoop(ctx)
}

// pollLoop runs the long polling fetch loop.
func (b *BotListener) pollLoop(ctx context.Context) {
	offset := 0

	for {
		select {
		case <-ctx.Done():
			b.logger.Info("Telegram bot listener stopped")
			return
		default:
		}

		updates, err := b.client.GetUpdates(ctx, offset, 20)
		if err != nil {
			// Back off on error
			select {
			case <-ctx.Done():
				return
			case <-time.After(5 * time.Second):
				continue
			}
		}

		for _, update := range updates {
			offset = update.UpdateID + 1

			if update.Message == nil || update.Message.Text == "" {
				continue
			}

			senderChatID := update.Message.Chat.ID
			senderName := update.Message.From.FirstName
			commandText := strings.TrimSpace(update.Message.Text)

			// Strict authorization check: Only allow configured TelegramChatID
			if senderChatID != b.cfg.TelegramChatID {
				b.logger.Warn("Unauthorized Telegram access attempt blocked",
					"sender_id", senderChatID,
					"sender_name", senderName,
					"text", commandText)

				unauthMsg := "<b>Access Denied</b>\n\nYou are not authorized to interact with this Home Station bot."
				_ = b.client.SendMessage(ctx, senderChatID, unauthMsg)
				continue
			}

			// Process authorized command
			b.handleCommand(ctx, senderChatID, commandText)
		}
	}
}

// handleCommand routes commands from the authorized user.
func (b *BotListener) handleCommand(ctx context.Context, chatID int64, text string) {
	cmd := strings.ToLower(strings.Fields(text)[0])

	switch cmd {
	case "/status", "/info", "status":
		t := b.telemetry.GetLatest()

		stateStr := "ONLINE"
		if !t.DeviceOnline {
			stateStr = "OFFLINE"
		}

		lightStr := "N/A"
		if t.LightStatus != "" {
			lightStr = fmt.Sprintf("%s (%d ADC)", t.LightStatus, t.LightRawADC)
		}

		bleStr := "Không phát hiện (Away)"
		if t.BLERssi > -100 && t.BLERssi != 0 {
			if t.BLERssi >= -60 {
				bleStr = fmt.Sprintf("Gần / Ở phòng (%d dBm)", t.BLERssi)
			} else if t.BLERssi >= -75 {
				bleStr = fmt.Sprintf("Vừa phải (%d dBm)", t.BLERssi)
			} else {
				bleStr = fmt.Sprintf("Yếu / Xa (%d dBm)", t.BLERssi)
			}
		}

		msg := fmt.Sprintf("🏠 <b>Home Station Live Telemetry</b>\n\n"+
			"<b>Status:</b> <code>%s</code>\n"+
			"<b>Device:</b> <code>%s</code>\n"+
			"<b>Temperature:</b> <code>%.1f°C</code>\n"+
			"<b>Humidity:</b> <code>%.1f%%</code>\n"+
			"<b>Smoke / Gas:</b> <code>%s (%d ADC)</code>\n"+
			"<b>Ambient Light:</b> <code>%s</code>\n"+
			"<b>BLE Proximity:</b> <code>%s</code>\n"+
			"<b>Voltage:</b> <code>%.3fV</code>\n"+
			"<b>Uptime:</b> <code>%d seconds</code>\n"+
			"<b>Last seen:</b> <code>%s</code>",
			t.SmokeStatus,
			stateStr,
			t.Temperature,
			t.Humidity,
			t.SmokeStatus,
			t.SmokeRawADC,
			lightStr,
			bleStr,
			t.SmokeVoltage,
			t.UptimeSec,
			t.LastSeen.Format("2006-01-02 15:04:05 UTC"))

		_ = b.client.SendMessage(ctx, chatID, msg)


	case "/ping", "ping":
		t := b.telemetry.GetLatest()
		msg := fmt.Sprintf("🏓 <b>Pong!</b>\n\nHome Station daemon is active.\nDevice online: <code>%t</code>\nUptime: <code>%d seconds</code>",
			t.DeviceOnline, t.UptimeSec)
		_ = b.client.SendMessage(ctx, chatID, msg)

	case "/help", "/start", "help":
		msg := "🤖 <b>Home Station Bot Commands</b>\n\n" +
			"• <code>/status</code> - Query real-time sensor measurements\n" +
			"• <code>/ping</code> - Check server liveness and uptime\n" +
			"• <code>/help</code> - Display available commands\n\n" +
			"<i>Automated alerts for fire, gas, and offline events are sent here automatically.</i>"
		_ = b.client.SendMessage(ctx, chatID, msg)

	default:
		msg := fmt.Sprintf("Unknown command: <code>%s</code>\nType <code>/help</code> to see available commands.", text)
		_ = b.client.SendMessage(ctx, chatID, msg)
	}
}
