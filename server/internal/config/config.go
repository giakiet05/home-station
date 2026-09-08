package config

import (
	"os"
	"strconv"
)

// Config encapsulates server, serial port, and notification runtime settings.
type Config struct {
	Port              string
	SerialPort        string
	BaudRate          int
	MockMode          bool
	LogLevel          string
	TelegramBotToken  string
	TelegramChatID    int64
	TelegramEnabled   bool
	AlertCooldownSec  int
	TempThresholdWarn float64
}

// Load loads configuration from environment variables with sensible defaults.
func Load() *Config {
	port := getEnv("PORT", "8080")
	serialPort := getEnv("SERIAL_PORT", "/dev/ttyACM0")
	baudRateStr := getEnv("BAUD_RATE", "115200")
	mockModeStr := getEnv("MOCK_MODE", "false")
	logLevel := getEnv("LOG_LEVEL", "INFO")
	telegramBotToken := getEnv("TELEGRAM_BOT_TOKEN", "")
	telegramChatIDStr := getEnv("TELEGRAM_CHAT_ID", "0")
	telegramEnabledStr := getEnv("TELEGRAM_ENABLED", "false")
	cooldownStr := getEnv("ALERT_COOLDOWN_SEC", "300")
	tempWarnStr := getEnv("TEMP_THRESHOLD_WARN", "40.0")

	baudRate, err := strconv.Atoi(baudRateStr)
	if err != nil {
		baudRate = 115200
	}

	mockMode, _ := strconv.ParseBool(mockModeStr)

	telegramChatID, _ := strconv.ParseInt(telegramChatIDStr, 10, 64)
	telegramEnabled, _ := strconv.ParseBool(telegramEnabledStr)
	// Auto-enable if bot token and chat ID are present and TELEGRAM_ENABLED is not explicitly false
	if telegramBotToken != "" && telegramChatID != 0 && telegramEnabledStr != "false" {
		telegramEnabled = true
	}

	alertCooldownSec, err := strconv.Atoi(cooldownStr)
	if err != nil || alertCooldownSec <= 0 {
		alertCooldownSec = 300 // default 5 minutes
	}

	tempThresholdWarn, err := strconv.ParseFloat(tempWarnStr, 64)
	if err != nil || tempThresholdWarn <= 0 {
		tempThresholdWarn = 40.0 // default 40°C
	}

	return &Config{
		Port:              port,
		SerialPort:        serialPort,
		BaudRate:          baudRate,
		MockMode:          mockMode,
		LogLevel:          logLevel,
		TelegramBotToken:  telegramBotToken,
		TelegramChatID:    telegramChatID,
		TelegramEnabled:   telegramEnabled,
		AlertCooldownSec:  alertCooldownSec,
		TempThresholdWarn: tempThresholdWarn,
	}
}

// getEnv retrieves an environment variable or returns a fallback default.
func getEnv(key, defaultValue string) string {
	if value, exists := os.LookupEnv(key); exists && value != "" {
		return value
	}
	return defaultValue
}
