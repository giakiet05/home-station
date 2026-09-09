#include <Arduino.h>
#include "config.h"
#include "sensor_manager.h"
#include "led_controller.h"
#include "watchdog_manager.h"
#include "ir_controller.h"

namespace {
    SensorManager sensorManager;
    LedController ledController;
    WatchdogManager watchdogManager(ledController);
    IrController irController;

    uint32_t lastReadTimestampMs = 0;
    String serialInputBuffer = "";
    SensorReadings currentReadings;
}

/**
 * @brief Processes incoming serial frames from the Homeserver Go daemon.
 */
void processSerialInput() {
    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (serialInputBuffer.length() > 0) {
                // Check if command is handled by IR controller
                if (!irController.handleCommand(serialInputBuffer)) {
                    String lowerBuf = serialInputBuffer;
                    lowerBuf.toLowerCase();
                    if (lowerBuf.indexOf("ping") >= 0) {
                        watchdogManager.feedHeartbeat();
                    }
                }
                serialInputBuffer = "";
            }
        } else {
            if (serialInputBuffer.length() < 128) {
                serialInputBuffer += c;
            }
        }
    }
}

/**
 * @brief System initialization entry point.
 */
void setup() {
    // Start serial communication
    Serial.begin(Config::SERIAL_BAUD_RATE);

    // Initialize LED indicators (immediately shows Blue booting state)
    ledController.init();

    // Wait briefly for USB CDC connection on ESP32-C3 Super Mini
    delay(2000);

    Serial.println();
    Serial.println("==================================================");
    Serial.println("    ESP32-C3 HOME STATION SYSTEM INITIALIZING     ");
    Serial.println("==================================================");

    // Initialize environmental sensors
    bool initSuccess = sensorManager.init();
    if (initSuccess) {
        Serial.println("[SYSTEM] Environmental sensors initialized.");
    } else {
        Serial.println("[WARN] Sensor initialization returned warning/fallback.");
    }

    // Initialize WiFi and hardware watchdog subsystem
    watchdogManager.init();

    // Initialize IR transmitter for LG AC control
    irController.init();

    Serial.println("[SYSTEM] System startup completed. Entering loop.");
    Serial.println("==================================================");
}

/**
 * @brief Main periodic execution loop.
 */
void loop() {
    // 1. Process serial commands (heartbeat pings from server)
    processSerialInput();

    // 2. Periodic sensor sampling and telemetry broadcast
    uint32_t currentTimestampMs = millis();
    if (currentTimestampMs - lastReadTimestampMs >= Config::SENSOR_READ_INTERVAL_MS) {
        lastReadTimestampMs = currentTimestampMs;

        currentReadings = sensorManager.read();
        sensorManager.printReadings(currentReadings);

        // Update Auto Night-Mode state on LED controller based on ambient light
        ledController.setNightMode(currentReadings.is_night_mode);
    }

    // 3. Periodic watchdog check and state updates
    bool isDanger = (currentReadings.air_status == AirQualityStatus::DANGER ||
                     currentReadings.air_status == AirQualityStatus::WARNING);
    watchdogManager.update(isDanger);

    // 4. Update LED blink animations
    ledController.update();

    // Yield brief slice to FreeRTOS scheduler
    delay(10);
}
