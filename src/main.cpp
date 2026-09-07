#include <Arduino.h>
#include "config.h"
#include "sensor_manager.h"

namespace {
    SensorManager sensorManager;
    uint32_t lastReadTimestampMs = 0;
}

/**
 * @brief System initialization entry point.
 */
void setup() {
    // Start serial communication
    Serial.begin(Config::SERIAL_BAUD_RATE);

    // Wait briefly for USB CDC connection on ESP32-C3 Super Mini
    delay(2000);

    Serial.println();
    Serial.println("==================================================");
    Serial.println("    ESP32-C3 HOME STATION SYSTEM INITIALIZING     ");
    Serial.println("==================================================");

    // Initialize all sensor peripherals
    bool initSuccess = sensorManager.init();
    if (initSuccess) {
        Serial.println("[SYSTEM] Environmental sensors initialized.");
    } else {
        Serial.println("[WARN] Partial or complete sensor initialization failure.");
    }

    Serial.println("[SYSTEM] System startup completed. Entering loop.");
    Serial.println("==================================================");
}

/**
 * @brief Main periodic execution loop.
 */
void loop() {
    uint32_t currentTimestampMs = millis();

    // Check if measurement interval has elapsed
    if (currentTimestampMs - lastReadTimestampMs >= Config::SENSOR_READ_INTERVAL_MS) {
        lastReadTimestampMs = currentTimestampMs;

        // Perform sensor acquisition and print telemetry
        SensorReadings readings = sensorManager.read();
        sensorManager.printReadings(readings);
    }

    // Yield to FreeRTOS scheduler
    delay(10);
}
