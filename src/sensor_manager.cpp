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
 * @brief Probes a specific pair of SDA and SCL pins with internal pull-ups enabled.
 * @param sdaPin GPIO number for SDA.
 * @param sclPin GPIO number for SCL.
 * @return uint8_t Number of I2C devices detected.
 */
static uint8_t probePinPair(int8_t sdaPin, int8_t sclPin) {
    pinMode(sdaPin, INPUT_PULLUP);
    pinMode(sclPin, INPUT_PULLUP);
    Wire.end();
    Wire.begin(sdaPin, sclPin, 100000);
    Wire.setTimeOut(25);

    uint8_t count = 0;
    for (uint8_t address = 1; address < 127; ++address) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C SCAN SUCCESS] Found device 0x%02X on SDA=GPIO%d, SCL=GPIO%d\n",
                          address, sdaPin, sclPin);
            count++;
        }
    }
    return count;
}

/**
 * @brief Performs a full exhaustive scan across all candidate GPIO pairs on ESP32-C3.
 * @param[out] foundSda Detected SDA pin.
 * @param[out] foundScl Detected SCL pin.
 * @return bool True if I2C devices were found, false otherwise.
 */
static bool autoDiscoverI2CPins(int8_t &foundSda, int8_t &foundScl) {
    Serial.println("[I2C] Starting comprehensive pin search across all GPIO combinations...");

    // Common pin pair candidates on ESP32-C3
    const int8_t candidatePairs[][2] = {
        {8, 9},   // Default Super Mini silkscreen (SDA=8, SCL=9)
        {9, 8},   // Swapped (SDA=9, SCL=8)
        {4, 5},   // Standard ESP32-C3 DevKit I2C default
        {5, 4},   // Swapped
        {6, 7},   // Top-left pins
        {7, 6},   // Swapped
        {10, 9},  // Mid-left pins
        {9, 10},  // Swapped
        {20, 21}, // Bottom-left pins
        {21, 20}, // Swapped
        {1, 2},   // Bottom-right pins
        {2, 1},   // Swapped
        {3, 4},   // Mid-right pins
        {4, 3}    // Swapped
    };

    for (const auto &pair : candidatePairs) {
        int8_t sda = pair[0];
        int8_t scl = pair[1];
        if (probePinPair(sda, scl) > 0) {
            foundSda = sda;
            foundScl = scl;
            Serial.printf("[I2C] Auto-discovery selected SDA=GPIO%d, SCL=GPIO%d\n", foundSda, foundScl);
            return true;
        }
    }

    Serial.println("[I2C] Comprehensive pin scan finished. No I2C response on any tested GPIO pair.");
    return false;
}

/**
 * @brief Checks if SDA and SCL lines are pulled HIGH or stuck LOW.
 */
static void checkPinStates(int8_t sdaPin, int8_t sclPin) {
    pinMode(sdaPin, INPUT_PULLUP);
    pinMode(sclPin, INPUT_PULLUP);
    delay(10);
    int sdaState = digitalRead(sdaPin);
    int sclState = digitalRead(sclPin);

    Serial.printf("[PIN CHECK] SDA (GPIO%d) = %s | SCL (GPIO%d) = %s\n",
                  sdaPin,
                  (sdaState == HIGH) ? "HIGH (Normal 3.3V)" : "LOW (Stuck/Shorted to GND!)",
                  sclPin,
                  (sclState == HIGH) ? "HIGH (Normal 3.3V)" : "LOW (Stuck/Shorted to GND!)");
}

/**
 * @brief Initializes I2C communication, sensor peripherals, and ADC pins.
 * @return true if at least one sensor was successfully initialized, false otherwise.
 */
bool SensorManager::init() {
    boot_timestamp_ms_ = millis();

    // Initialize MQ-2 analog pin
    pinMode(Config::PIN_MQ2_AO, INPUT);

    // Check physical electrical level on I2C bus pins
    checkPinStates(Config::PIN_I2C_SDA, Config::PIN_I2C_SCL);

    int8_t activeSda = Config::PIN_I2C_SDA;
    int8_t activeScl = Config::PIN_I2C_SCL;

    // Initialize DHT11
    dht_.begin();
    dht_initialized_ = true;
    Serial.printf("[INFO] DHT11 sensor initialized on GPIO%d.\n", Config::PIN_DHT_DATA);

    // Run auto-discovery scan for I2C devices
    if (!autoDiscoverI2CPins(activeSda, activeScl)) {
        // Fall back to default config pins
        Wire.end();
        Wire.begin(Config::PIN_I2C_SDA, Config::PIN_I2C_SCL, 100000);
    }

    // Initialize AHT20 if present
    if (aht20_.begin(&Wire)) {
        aht20_initialized_ = true;
        Serial.println("[INFO] AHT20 sensor initialized successfully.");
    } else {
        Serial.println("[INFO] AHT20 sensor not present on I2C bus.");
    }

    // Initialize BMP280 if present (check default 0x77, fallback to 0x76)
    if (bmp280_.begin(Config::I2C_ADDR_BMP280_DEFAULT, BMP280_CHIPID)) {
        bmp280_initialized_ = true;
        Serial.println("[INFO] BMP280 sensor initialized at address 0x77.");
    } else if (bmp280_.begin(Config::I2C_ADDR_BMP280_ALT, BMP280_CHIPID)) {
        bmp280_initialized_ = true;
        Serial.println("[INFO] BMP280 sensor initialized at address 0x76.");
    } else {
        Serial.println("[INFO] BMP280 sensor not present on I2C bus.");
    }

    if (bmp280_initialized_) {
        // Configure standard sampling parameters for indoor monitoring
        bmp280_.setSampling(
            Adafruit_BMP280::MODE_NORMAL,
            Adafruit_BMP280::SAMPLING_X2,
            Adafruit_BMP280::SAMPLING_X16,
            Adafruit_BMP280::FILTER_X16,
            Adafruit_BMP280::STANDBY_MS_500
        );
    }

    return (dht_initialized_ || aht20_initialized_ || bmp280_initialized_);
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
