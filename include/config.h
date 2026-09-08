#pragma once

#include <cstdint>

/**
 * @file config.h
 * @brief System hardware pin definitions and configuration parameters for ESP32-C3 Home Station.
 */

namespace Config {
    // RGB Status LED Pin Configuration (Common Cathode)
    constexpr int8_t PIN_RGB_RED = 5;      // GPIO5
    constexpr int8_t PIN_RGB_GREEN = 6;    // GPIO6
    constexpr int8_t PIN_RGB_BLUE = 7;     // GPIO7
    constexpr int8_t PIN_BUILTIN_LED = 8;  // GPIO8 (Active LOW)

    // MQ-2 Gas/Smoke Sensor Pin Configuration
    constexpr int8_t PIN_MQ2_AO = 0;       // GPIO0 (ADC1_CH0)

    // DHT11 Temperature & Humidity Sensor Pin Configuration
    constexpr int8_t PIN_DHT_DATA = 4;     // GPIO4
    constexpr uint8_t DHT_TYPE = 11;       // DHT11 sensor type

    // Fallback/Optional I2C Addresses & Parameters
    constexpr int8_t PIN_I2C_SDA = 8;
    constexpr int8_t PIN_I2C_SCL = 9;
    constexpr uint8_t I2C_ADDR_AHT20 = 0x38;
    constexpr uint8_t I2C_ADDR_BMP280_DEFAULT = 0x77;
    constexpr uint8_t I2C_ADDR_BMP280_ALT = 0x76;
    constexpr float SEA_LEVEL_PRESSURE_HPA = 1013.25f;

    // WiFi Configuration for Hardware Watchdog
    constexpr const char* WIFI_SSID = "REDACTED_WIFI_SSID";
    constexpr const char* WIFI_PASSWORD = "REDACTED_WIFI_PASSWORD";

    // Telegram Bot Configuration for Hardware Watchdog
    constexpr const char* TELEGRAM_BOT_TOKEN = "REDACTED_TELEGRAM_BOT_TOKEN";
    constexpr const char* TELEGRAM_CHAT_ID = "8465841006";

    // Hardware ADC parameters for ESP32-C3
    constexpr float ADC_REF_VOLTAGE = 3.3f;
    constexpr uint16_t ADC_MAX_RESOLUTION = 4095; // 12-bit ADC

    // Thresholds for gas/smoke detection (Raw ADC voltage and levels)
    constexpr float MQ2_VOLTAGE_THRESHOLD_WARN = 1.2f;
    constexpr float MQ2_VOLTAGE_THRESHOLD_DANGER = 2.0f;

    // Timing parameters (in milliseconds)
    constexpr uint32_t SERIAL_BAUD_RATE = 115200;
    constexpr uint32_t SENSOR_READ_INTERVAL_MS = 2000;
    constexpr uint32_t MQ2_WARMUP_DURATION_MS = 20000;
    constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 25000; // 25s timeout for Homeserver
}

