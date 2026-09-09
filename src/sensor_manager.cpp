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

    // Initialize MQ-2 and LDR analog pins
    pinMode(Config::PIN_MQ2_AO, INPUT);
    pinMode(Config::PIN_LDR_AO, INPUT);

    // Initialize DHT11
    dht_.begin();
    dht_initialized_ = true;
    Serial.printf("[INFO] DHT11 sensor initialized on GPIO%d.\n", Config::PIN_DHT_DATA);
    Serial.printf("[INFO] LDR ambient light sensor active on GPIO%d.\n", Config::PIN_LDR_AO);

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
 * @brief Evaluates ambient light level and night mode state from LDR ADC.
 * @param adc Raw 12-bit ADC reading from LDR pin.
 * @param isNightMode Reference output set to true when lighting is dark.
 * @return AmbientLightStatus Current ambient light classification.
 */
AmbientLightStatus SensorManager::evaluateLightStatus(uint16_t adc, bool &isNightMode) {
    if (adc < Config::LDR_NIGHT_THRESHOLD_ADC) {
        isNightMode = true;
        return AmbientLightStatus::DARK;
    }

    isNightMode = false;
    if (adc >= Config::LDR_SUNNY_THRESHOLD_ADC) {
        return AmbientLightStatus::DIRECT_SUNLIGHT;
    } else if (adc >= 2600) {
        return AmbientLightStatus::BRIGHT;
    } else if (adc >= 1000) {
        return AmbientLightStatus::INDOOR_LIGHT;
    }
    return AmbientLightStatus::DIM;
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

    // 5. Read LDR (Ambient Light ADC)
    readings.light_raw_adc = analogRead(Config::PIN_LDR_AO);
    readings.light_percentage = (static_cast<float>(readings.light_raw_adc) / static_cast<float>(Config::ADC_MAX_RESOLUTION)) * 100.0f;
    readings.light_status = evaluateLightStatus(readings.light_raw_adc, readings.is_night_mode);

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
 * @brief Converts an AmbientLightStatus enum value into a readable string.
 * @param status The status enum to convert.
 * @return const char* String representation of the light status.
 */
const char* SensorManager::ambientLightStatusToString(AmbientLightStatus status) {
    switch (status) {
        case AmbientLightStatus::DARK:
            return "DARK (Night Mode)";
        case AmbientLightStatus::DIM:
            return "DIM";
        case AmbientLightStatus::INDOOR_LIGHT:
            return "INDOOR LIGHT";
        case AmbientLightStatus::BRIGHT:
            return "BRIGHT";
        case AmbientLightStatus::DIRECT_SUNLIGHT:
            return "DIRECT SUNLIGHT";
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
    const char *lightStatusStr = ambientLightStatusToString(readings.light_status);
    const char *isNightStr = readings.is_night_mode ? "true" : "false";
    const char *presenceStr = readings.presence_detected ? "true" : "false";
    const char *triggerStr = readings.presence_trigger ? readings.presence_trigger : "NONE";

    // Single-line structured JSON payload for downstream collectors
    Serial.printf("{\"temp\":%.1f,\"humidity\":%.1f,\"smoke_raw\":%u,\"smoke_voltage\":%.3f,\"smoke_pct\":%.1f,\"status\":\"%s\",\"light_raw\":%u,\"light_pct\":%.1f,\"light_status\":\"%s\",\"is_night\":%s,\"presence\":%s,\"ble_rssi\":%d,\"presence_trigger\":\"%s\",\"uptime_ms\":%lu}\n",
                  temp,
                  hum,
                  readings.mq2_raw_adc,
                  readings.mq2_voltage,
                  readings.mq2_percentage,
                  statusStr,
                  readings.light_raw_adc,
                  readings.light_percentage,
                  lightStatusStr,
                  isNightStr,
                  presenceStr,
                  readings.ble_rssi,
                  triggerStr,
                  millis());
}
