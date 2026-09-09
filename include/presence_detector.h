#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "config.h"

/**
 * @file presence_detector.h
 * @brief Combines LDR light transitions and BLE beacon proximity to detect room entry.
 */

class PresenceDetector : public NimBLEAdvertisedDeviceCallbacks {
public:
    /**
     * @brief Constructs a new PresenceDetector instance.
     */
    PresenceDetector();

    /**
     * @brief Initializes the lightweight NimBLE hardware scanner.
     */
    void init();

    /**
     * @brief Evaluates sensor state and periodic BLE scans to detect human presence.
     * @param currentLightAdc Current LDR analog reading.
     * @param isNightMode Whether the room is currently dark.
     */
    void update(uint16_t currentLightAdc, bool isNightMode);

    /**
     * @brief Checks if a fresh room presence event was triggered.
     * @return true if room entry was recently detected, false otherwise.
     */
    bool isPresenceDetected() const;

    /**
     * @brief Gets the strongest observed BLE signal strength (RSSI in dBm).
     * @return int8_t RSSI value.
     */
    int8_t getStrongestRssi() const;

    /**
     * @brief Retrieves the human-readable trigger reason.
     * @return const char* Trigger description.
     */
    const char* getTriggerReason() const;

    /**
     * @brief Clears the active presence flag after dispatching.
     */
    void clearPresence();

    /**
     * @brief Callback invoked when a BLE advertising packet is intercepted.
     * @param advertisedDevice Pointer to discovered BLE advertised device.
     */
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;

private:
    NimBLEScan* bleScan_;
    uint32_t lastBleScanMs_;
    uint32_t lastLightTriggerMs_;
    uint32_t lastBleTriggerMs_;
    uint16_t prevLightAdc_;
    bool wasNightMode_;
    bool presenceDetected_;
    int8_t strongestRssi_;
    int8_t scanStrongestRssi_;
    String triggerReason_;

    /**
     * @brief Activates the presence state machine flag.
     * @param reason Trigger cause description.
     */
    void triggerPresence(const char* reason);
};

