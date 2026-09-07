#pragma once

#include <cstdint>

/**
 * @file config.h
 * @brief System hardware pin definitions and configuration parameters for ESP32-C3 Home Station.
 */

namespace Config {
    // I2C Pin Configuration for ESP32-C3 Super Mini
    constexpr int8_t PIN_I2C_SDA = 8;
    constexpr int8_t PIN_I2C_SCL = 9;

    // MQ-2 Gas/Smoke Sensor Pin Configuration
    constexpr int8_t PIN_MQ2_AO = 0;  // GPIO0 (ADC1_CH0)

    // DHT11 Temperature & Humidity Sensor Pin Configuration
    constexpr int8_t PIN_DHT_DATA = 4; // GPIO4
    constexpr uint8_t DHT_TYPE = 11;    // DHT11 sensor type

    // Sensor I2C Addresses
    constexpr uint8_t I2C_ADDR_AHT20 = 0x38;
    constexpr uint8_t I2C_ADDR_BMP280_DEFAULT = 0x77;
    constexpr uint8_t I2C_ADDR_BMP280_ALT = 0x76;

    // Environmental calculation parameters
    constexpr float SEA_LEVEL_PRESSURE_HPA = 1013.25f;

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
}
