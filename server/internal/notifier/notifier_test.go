package notifier

import (
	"context"
	"log/slog"
	"os"
	"sync"
	"testing"
	"time"

	"github.com/giakiet05/home-station/server/internal/config"
	"github.com/giakiet05/home-station/server/internal/model"
)

type mockTelegramClient struct {
	mu       sync.Mutex
	messages []struct {
		ChatID int64
		Text   string
	}
	updates []TelegramUpdate
}

func (m *mockTelegramClient) SendMessage(ctx context.Context, chatID int64, text string) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.messages = append(m.messages, struct {
		ChatID int64
		Text   string
	}{ChatID: chatID, Text: text})
	return nil
}

func (m *mockTelegramClient) GetUpdates(ctx context.Context, offset int, timeoutSec int) ([]TelegramUpdate, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	res := m.updates
	m.updates = nil
	return res, nil
}

type mockTelemetrySource struct {
	telemetry model.Telemetry
}

func (m *mockTelemetrySource) GetLatest() model.Telemetry {
	return m.telemetry
}

func TestAlertManager_DangerAndRecoveryTransitions(t *testing.T) {
	cfg := &config.Config{
		TelegramEnabled:  true,
		TelegramChatID:   123456789,
		AlertCooldownSec: 300,
	}
	client := &mockTelegramClient{}
	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))
	am := NewAlertManager(cfg, client, logger)

	ctx := context.Background()

	// 1. Initial normal state - should NOT trigger alert
	am.ProcessTelemetry(ctx, model.Telemetry{
		DeviceOnline: true,
		SmokeStatus:  "NORMAL",
		Temperature:  30.0,
	})

	time.Sleep(50 * time.Millisecond)
	if len(client.messages) != 0 {
		t.Fatalf("expected 0 messages for normal state, got %d", len(client.messages))
	}

	// 2. Danger detected - MUST trigger emergency alert
	am.ProcessTelemetry(ctx, model.Telemetry{
		DeviceOnline: true,
		SmokeStatus:  "DANGER",
		SmokeRawADC:  3200,
		SmokeVoltage: 2.5,
		Temperature:  35.0,
		Humidity:     70.0,
	})

	time.Sleep(50 * time.Millisecond)
	client.mu.Lock()
	if len(client.messages) != 1 {
		t.Fatalf("expected 1 emergency alert message, got %d", len(client.messages))
	}
	client.mu.Unlock()

	// 3. Danger continues within cooldown - MUST NOT send duplicate alert
	am.ProcessTelemetry(ctx, model.Telemetry{
		DeviceOnline: true,
		SmokeStatus:  "DANGER",
		SmokeRawADC:  3250,
		SmokeVoltage: 2.6,
		Temperature:  35.5,
		Humidity:     70.0,
	})

	time.Sleep(50 * time.Millisecond)
	client.mu.Lock()
	if len(client.messages) != 1 {
		t.Fatalf("expected cooldown to prevent duplicate alert, got %d messages", len(client.messages))
	}
	client.mu.Unlock()

	// 4. Recovery to Normal - MUST trigger recovery notification
	am.ProcessTelemetry(ctx, model.Telemetry{
		DeviceOnline: true,
		SmokeStatus:  "NORMAL",
		SmokeRawADC:  350,
		SmokeVoltage: 0.28,
		Temperature:  31.0,
		Humidity:     72.0,
	})

	time.Sleep(50 * time.Millisecond)
	client.mu.Lock()
	if len(client.messages) != 2 {
		t.Fatalf("expected 2 messages total (1 danger, 1 recovery), got %d", len(client.messages))
	}
	client.mu.Unlock()
}

func TestAlertManager_DeviceOfflineTransition(t *testing.T) {
	cfg := &config.Config{
		TelegramEnabled:  true,
		TelegramChatID:   123456789,
		AlertCooldownSec: 300,
	}
	client := &mockTelegramClient{}
	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))
	am := NewAlertManager(cfg, client, logger)

	ctx := context.Background()

	// 1. Device online
	am.ProcessTelemetry(ctx, model.Telemetry{
		DeviceOnline: true,
		SmokeStatus:  "NORMAL",
	})

	// 2. Device goes offline
	am.ProcessTelemetry(ctx, model.Telemetry{
		DeviceOnline: false,
		SmokeStatus:  "OFFLINE",
	})

	time.Sleep(50 * time.Millisecond)
	client.mu.Lock()
	if len(client.messages) != 1 {
		t.Fatalf("expected 1 offline alert, got %d", len(client.messages))
	}
	client.mu.Unlock()

	// 3. Device comes back online - recovery message
	am.ProcessTelemetry(ctx, model.Telemetry{
		DeviceOnline: true,
		SmokeStatus:  "NORMAL",
	})

	time.Sleep(50 * time.Millisecond)
	client.mu.Lock()
	if len(client.messages) != 2 {
		t.Fatalf("expected 2 messages (offline and online recovery), got %d", len(client.messages))
	}
	client.mu.Unlock()
}

func TestBotListener_AuthorizationCheck(t *testing.T) {
	cfg := &config.Config{
		TelegramEnabled:  true,
		TelegramBotToken: "test-token",
		TelegramChatID:   999999, // Authorized user ID
	}
	client := &mockTelegramClient{}
	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))
	telemetrySrc := &mockTelemetrySource{
		telemetry: model.Telemetry{
			DeviceOnline: true,
			Temperature:  32.0,
			Humidity:     75.0,
			SmokeRawADC:  350,
			SmokeStatus:  "NORMAL",
			UptimeSec:    100,
		},
	}

	bot := NewBotListener(cfg, client, telemetrySrc, logger)

	// 1. Unauthorized user message
	unauthorizedUpdate := TelegramUpdate{
		UpdateID: 1,
		Message: &struct {
			MessageID int `json:"message_id"`
			From      struct {
				ID        int64  `json:"id"`
				FirstName string `json:"first_name"`
				Username  string `json:"username"`
			} `json:"from"`
			Chat struct {
				ID    int64  `json:"id"`
				Type  string `json:"type"`
				Title string `json:"title"`
			} `json:"chat"`
			Date int64  `json:"date"`
			Text string `json:"text"`
		}{
			MessageID: 10,
			Text:      "/status",
		},
	}
	unauthorizedUpdate.Message.Chat.ID = 111111 // Unauthorized ID

	client.mu.Lock()
	client.updates = []TelegramUpdate{unauthorizedUpdate}
	client.mu.Unlock()

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Run 1 polling cycle
	go bot.pollLoop(ctx)
	time.Sleep(50 * time.Millisecond)

	client.mu.Lock()
	if len(client.messages) != 1 {
		t.Fatalf("expected 1 access denied reply, got %d", len(client.messages))
	}
	if client.messages[0].ChatID != 111111 {
		t.Errorf("expected access denied sent to unauthorized user 111111, got %d", client.messages[0].ChatID)
	}
	client.messages = nil
	client.mu.Unlock()

	// 2. Authorized user message
	authorizedUpdate := unauthorizedUpdate
	authorizedUpdate.UpdateID = 2
	authorizedUpdate.Message.Chat.ID = 999999 // Authorized ID
	authorizedUpdate.Message.Text = "/status"

	client.mu.Lock()
	client.updates = []TelegramUpdate{authorizedUpdate}
	client.mu.Unlock()

	time.Sleep(50 * time.Millisecond)

	client.mu.Lock()
	if len(client.messages) != 1 {
		t.Fatalf("expected 1 telemetry reply for authorized user, got %d", len(client.messages))
	}
	if client.messages[0].ChatID != 999999 {
		t.Errorf("expected telemetry reply sent to authorized user 999999, got %d", client.messages[0].ChatID)
	}
	client.mu.Unlock()
}
