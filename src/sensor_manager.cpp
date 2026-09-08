#include "sensor_manager.h"

/**
 * @brief Constructs a new SensorManager instance and resets member states.
 */
SensorManager::SensorManager()
    : dht_(Config::PIN_DHT_DATA, Config::DHT_TYPE),
      dht_initialized_(false),
      aht20_initialized_(false),
      bmp280_initialized_(false),
      boot_timestamp_ms_(0) {}



/**
 * @brief Initializes I2C communication, sensor peripherals, and ADC pins.
 * @return true if at least one sensor was successfully initialized, false otherwise.
 */
bool SensorManager::init() {
    boot_timestamp_ms_ = millis();

    // Initialize MQ-2 analog pin
    pinMode(Config::PIN_MQ2_AO, INPUT);

    // Initialize DHT11
    dht_.begin();
    dht_initialized_ = true;
    Serial.printf("[INFO] DHT11 sensor initialized on GPIO%d.\n", Config::PIN_DHT_DATA);

    return dht_initialized_;
}

/**
 * @brief Evaluates smoke and gas hazard level based on voltage reading and system uptime.
 * @param voltage Analog voltage measured at MQ-2 output pin.
 * @param isWarmingUp Reference output indicating if heater warmup period is active.
 * @return AirQualityStatus Current air quality condition.
 */
AirQualityStatus SensorManager::evaluateAirQuality(float voltage, bool &isWarmingUp) {
    if (millis() - boot_timestamp_ms_ < Config::MQ2_WARMUP_DURATION_MS) {
        isWarmingUp = true;
        return AirQualityStatus::WARMING_UP;
    }

    isWarmingUp = false;
    if (voltage >= Config::MQ2_VOLTAGE_THRESHOLD_DANGER) {
        return AirQualityStatus::DANGER;
    } else if (voltage >= Config::MQ2_VOLTAGE_THRESHOLD_WARN) {
        return AirQualityStatus::WARNING;
    }

    return AirQualityStatus::NORMAL;
}

/**
 * @brief Acquires a fresh snapshot of all sensor metrics.
 * @return SensorReadings struct containing aggregated sensor values and validity flags.
 */
SensorReadings SensorManager::read() {
    SensorReadings readings = {};

    // 1. Read DHT11 (Temperature & Humidity)
    if (dht_initialized_) {
        float dht_h = dht_.readHumidity();
        float dht_t = dht_.readTemperature();
        if (!isnan(dht_h) && !isnan(dht_t)) {
            readings.temperature_c = dht_t;
            readings.humidity_pct = dht_h;
            readings.dht_valid = true;
        }
    }

    // 2. Read AHT20 (Higher precision if available)
    if (aht20_initialized_) {
        sensors_event_t humidity_event;
        sensors_event_t temp_event;
        if (aht20_.getEvent(&humidity_event, &temp_event)) {
            readings.temperature_c = temp_event.temperature;
            readings.humidity_pct = humidity_event.relative_humidity;
            readings.aht20_valid = true;
        }
    }

    // 3. Read BMP280 (Pressure & Secondary Temperature)
    if (bmp280_initialized_) {
        readings.bmp_temperature_c = bmp280_.readTemperature();
        readings.pressure_hpa = bmp280_.readPressure() / 100.0f; // Convert Pa to hPa
        readings.altitude_m = bmp280_.readAltitude(Config::SEA_LEVEL_PRESSURE_HPA);
        readings.bmp280_valid = true;
    }

    // 4. Read MQ-2 (Smoke & Gas ADC)
    readings.mq2_raw_adc = analogRead(Config::PIN_MQ2_AO);
    readings.mq2_voltage = (static_cast<float>(readings.mq2_raw_adc) / static_cast<float>(Config::ADC_MAX_RESOLUTION)) * Config::ADC_REF_VOLTAGE;
    readings.mq2_percentage = (static_cast<float>(readings.mq2_raw_adc) / static_cast<float>(Config::ADC_MAX_RESOLUTION)) * 100.0f;
    readings.mq2_valid = true;

    // Evaluate hazard status
    readings.air_status = evaluateAirQuality(readings.mq2_voltage, readings.is_warming_up);

    return readings;
}

/**
 * @brief Converts an AirQualityStatus enum value into a readable string.
 * @param status The status enum to convert.
 * @return const char* String representation of the status.
 */
const char* SensorManager::airQualityStatusToString(AirQualityStatus status) {
    switch (status) {
        case AirQualityStatus::WARMING_UP:
            return "WARMING UP";
        case AirQualityStatus::NORMAL:
            return "NORMAL / SAFE";
        case AirQualityStatus::WARNING:
            return "WARNING (Elevated Gas)";
        case AirQualityStatus::DANGER:
            return "DANGER (Smoke/Gas Detected)";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Logs the sensor readings as structured JSON string to Serial.
 * @param readings Constant reference to the collected sensor dataset.
 */
void SensorManager::printReadings(const SensorReadings &readings) {
    float temp = (readings.dht_valid || readings.aht20_valid) ? readings.temperature_c : 0.0f;
    float hum = (readings.dht_valid || readings.aht20_valid) ? readings.humidity_pct : 0.0f;
    const char *statusStr = airQualityStatusToString(readings.air_status);

    // Single-line structured JSON payload for downstream collectors
    Serial.printf("{\"temp\":%.1f,\"humidity\":%.1f,\"smoke_raw\":%u,\"smoke_voltage\":%.3f,\"smoke_pct\":%.1f,\"status\":\"%s\",\"uptime_ms\":%lu}\n",
                  temp,
                  hum,
                  readings.mq2_raw_adc,
                  readings.mq2_voltage,
                  readings.mq2_percentage,
                  statusStr,
                  millis());
}
