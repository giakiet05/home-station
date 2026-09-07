package config

import (
	"os"
	"strconv"
)

// Config encapsulates server and serial port runtime settings.
type Config struct {
	Port       string
	SerialPort string
	BaudRate   int
	MockMode   bool
	LogLevel   string
}

// Load loads configuration from environment variables with sensible defaults.
func Load() *Config {
	port := getEnv("PORT", "8080")
	serialPort := getEnv("SERIAL_PORT", "/dev/ttyACM0")
	baudRateStr := getEnv("BAUD_RATE", "115200")
	mockModeStr := getEnv("MOCK_MODE", "false")
	logLevel := getEnv("LOG_LEVEL", "INFO")

	baudRate, err := strconv.Atoi(baudRateStr)
	if err != nil {
		baudRate = 115200
	}

	mockMode, _ := strconv.ParseBool(mockModeStr)

	return &Config{
		Port:       port,
		SerialPort: serialPort,
		BaudRate:   baudRate,
		MockMode:   mockMode,
		LogLevel:   logLevel,
	}
}

// getEnv retrieves an environment variable or returns a fallback default.
func getEnv(key, defaultValue string) string {
	if value, exists := os.LookupEnv(key); exists && value != "" {
		return value
	}
	return defaultValue
}
