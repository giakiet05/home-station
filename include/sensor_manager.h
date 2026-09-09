#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include "config.h"

/**
 * @enum AirQualityStatus
 * @brief Categorized hazard levels derived from MQ-2 sensor readings.
 */
enum class AirQualityStatus {
    WARMING_UP,
    NORMAL,
    WARNING,
    DANGER
};

/**
 * @enum AmbientLightStatus
 * @brief Categorized ambient light conditions from LDR sensor readings.
 */
enum class AmbientLightStatus {
    DARK,               // Pitch dark / Night mode (<350 ADC)
    DIM,                // Dim room / Twilight (350-1000 ADC)
    INDOOR_LIGHT,       // Normal indoor lamp / Daylight (1000-2600 ADC)
    BRIGHT,             // Bright room / Window daylight (2600-3200 ADC)
    DIRECT_SUNLIGHT     // Direct sunlight hitting room (>3200 ADC)
};

/**
 * @struct SensorReadings
 * @brief Aggregated telemetry data from all connected environment sensors.
 */
struct SensorReadings {
    float temperature_c;        /**< Ambient temperature in Celsius */
    float humidity_pct;         /**< Relative humidity in percentage */
    float pressure_hpa;         /**< Barometric pressure in hectopascals (from BMP280) */
    float altitude_m;           /**< Estimated altitude above sea level in meters */
    float bmp_temperature_c;    /**< Secondary temperature reference in Celsius (from BMP280) */

    uint16_t mq2_raw_adc;       /**< Raw 12-bit ADC reading from MQ-2 */
    float mq2_voltage;          /**< Computed voltage at MQ-2 analog pin */
    float mq2_percentage;       /**< Estimated gas concentration percentage (0-100%) */

    uint16_t light_raw_adc;     /**< Raw 12-bit ADC reading from LDR (GPIO1) */
    float light_percentage;     /**< Computed light level percentage (0-100%) */
    bool is_night_mode;         /**< True if ambient light is below night threshold */
    AmbientLightStatus light_status; /**< Categorized ambient illumination status */

    bool dht_valid;             /**< Flag indicating successful DHT11 sample */
    bool aht20_valid;           /**< Flag indicating successful AHT20 sample */
    bool bmp280_valid;          /**< Flag indicating successful BMP280 sample */
    bool mq2_valid;             /**< Flag indicating successful MQ-2 sample */
    bool is_warming_up;         /**< Flag indicating MQ-2 internal heater warmup phase */

    AirQualityStatus air_status;/**< Evaluated air hazard condition */
};

/**
 * @class SensorManager
 * @brief Manages I2C bus lifecycle, DHT11, and unified data acquisition.
 */
class SensorManager {
public:
    /**
     * @brief Constructs a new SensorManager instance.
     */
    SensorManager();

    /**
     * @brief Initializes I2C communication, DHT11, and ADC pins.
     * @return true if at least one sensor was successfully initialized, false otherwise.
     */
    bool init();

    /**
     * @brief Acquires a fresh snapshot of all sensor metrics.
     * @return SensorReadings struct containing aggregated sensor values and validity flags.
     */
    SensorReadings read();

    /**
     * @brief Logs the formatted sensor readings to the primary Serial stream.
     * @param readings Constant reference to the collected sensor dataset.
     */
    void printReadings(const SensorReadings &readings);

    /**
     * @brief Converts an AirQualityStatus enum value into a readable string.
     * @param status The status enum to convert.
     * @return const char* String representation of the status.
     */
    static const char* airQualityStatusToString(AirQualityStatus status);

    /**
     * @brief Converts an AmbientLightStatus enum value into a readable string.
     * @param status The status enum to convert.
     * @return const char* String representation of the light status.
     */
    static const char* ambientLightStatusToString(AmbientLightStatus status);

private:
    DHT dht_;
    Adafruit_AHTX0 aht20_;
    Adafruit_BMP280 bmp280_;

    bool dht_initialized_;
    bool aht20_initialized_;
    bool bmp280_initialized_;
    uint32_t boot_timestamp_ms_;

    /**
     * @brief Evaluates smoke and gas hazard level based on voltage reading and system uptime.
     * @param voltage Analog voltage measured at MQ-2 output pin.
     * @param isWarmingUp Reference output indicating if heater warmup period is active.
     * @return AirQualityStatus Current air quality condition.
     */
    AirQualityStatus evaluateAirQuality(float voltage, bool &isWarmingUp);

    /**
     * @brief Evaluates ambient light level and night mode state from LDR ADC.
     * @param adc Raw 12-bit ADC reading from LDR pin.
     * @param isNightMode Reference output set to true when lighting is dark.
     * @return AmbientLightStatus Current ambient light classification.
     */
    AmbientLightStatus evaluateLightStatus(uint16_t adc, bool &isNightMode);
};
