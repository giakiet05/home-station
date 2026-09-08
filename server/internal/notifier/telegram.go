package notifier

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"time"
)

// TelegramUpdate represents an incoming update message from the Telegram Bot API.
type TelegramUpdate struct {
	UpdateID int `json:"update_id"`
	Message  *struct {
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
	} `json:"message"`
}

// TelegramAPIResponse represents a standard API response wrapper from Telegram.
type TelegramAPIResponse struct {
	OK          bool             `json:"ok"`
	Result      []TelegramUpdate `json:"result,omitempty"`
	Description string           `json:"description,omitempty"`
}

// TelegramClient defines operations for communicating with the Telegram Bot API.
type TelegramClient interface {
	SendMessage(ctx context.Context, chatID int64, text string) error
	GetUpdates(ctx context.Context, offset int, timeoutSec int) ([]TelegramUpdate, error)
}

// HTTPTelegramClient implements TelegramClient using standard HTTP requests.
type HTTPTelegramClient struct {
	botToken   string
	httpClient *http.Client
	logger     *slog.Logger
}

// NewHTTPTelegramClient constructs a new Telegram client.
func NewHTTPTelegramClient(botToken string, logger *slog.Logger) *HTTPTelegramClient {
	return &HTTPTelegramClient{
		botToken: botToken,
		httpClient: &http.Client{
			Timeout: 30 * time.Second,
		},
		logger: logger,
	}
}

// SendMessage delivers a text message to a specific Telegram chat ID.
func (c *HTTPTelegramClient) SendMessage(ctx context.Context, chatID int64, text string) error {
	if c.botToken == "" || chatID == 0 {
		return fmt.Errorf("bot token or chat ID is empty")
	}

	apiURL := fmt.Sprintf("https://api.telegram.org/bot%s/sendMessage", c.botToken)

	payload := map[string]any{
		"chat_id":    chatID,
		"text":       text,
		"parse_mode": "HTML",
	}

	bodyBytes, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to marshal telegram payload: %w", err)
	}

	req, err := http.NewRequestWithContext(ctx, http.MethodPost, apiURL, bytes.NewReader(bodyBytes))
	if err != nil {
		return fmt.Errorf("failed to create telegram request: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return fmt.Errorf("telegram request failed: %w", err)
	}
	defer resp.Body.Close()

	respBody, _ := io.ReadAll(resp.Body)
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("telegram api error (status %d): %s", resp.StatusCode, string(respBody))
	}

	return nil
}

// GetUpdates polls the Telegram Bot API for incoming updates using long polling.
func (c *HTTPTelegramClient) GetUpdates(ctx context.Context, offset int, timeoutSec int) ([]TelegramUpdate, error) {
	if c.botToken == "" {
		return nil, fmt.Errorf("bot token is empty")
	}

	apiURL := fmt.Sprintf("https://api.telegram.org/bot%s/getUpdates?offset=%d&timeout=%d", c.botToken, offset, timeoutSec)

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, apiURL, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create getUpdates request: %w", err)
	}

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("getUpdates request failed: %w", err)
	}
	defer resp.Body.Close()

	var apiResp TelegramAPIResponse
	if err := json.NewDecoder(resp.Body).Decode(&apiResp); err != nil {
		return nil, fmt.Errorf("failed to decode getUpdates response: %w", err)
	}

	if !apiResp.OK {
		return nil, fmt.Errorf("telegram api returned ok=false: %s", apiResp.Description)
	}

	return apiResp.Result, nil
}
