#pragma once

#include <Arduino.h>
#include "led_controller.h"

/**
 * @class WatchdogManager
 * @brief Manages WiFi connection, serial heartbeat watchdog, and Telegram emergency notifications.
 */
class WatchdogManager {
public:
    WatchdogManager(LedController& ledCtrl);

    /**
     * @brief Initializes WiFi subsystem in non-blocking mode.
     */
    void init();

    /**
     * @brief Feeds the hardware watchdog timer upon receiving a heartbeat frame from Homeserver.
     */
    void feedHeartbeat();

    /**
     * @brief Periodic update loop managing WiFi reconnects, watchdog timeout checks, and LED state.
     * @param smokeDetected True if current sensor readings indicate gas/smoke hazard.
     */
    void update(bool smokeDetected);

    /**
     * @brief Dispatches an immediate Markdown notification to the configured Telegram chat via HTTPS.
     * @param message UTF-8 text message payload.
     * @return True if HTTP 200 was returned by Telegram API.
     */
    bool sendTelegramAlert(const String& message);

    /**
     * @brief Checks if Homeserver is currently determined to be online and responding.
     */
    bool isServerOnline() const;

private:
    LedController& ledController;
    uint32_t lastHeartbeatMs;
    uint32_t lastWifiCheckMs;
    bool serverOnline;
    bool hadInitialHeartbeat;
    bool alertDispatchedForHang;
    bool pendingHangAlert;
    bool pendingRecoveryAlert;

    void ensureWiFiConnected();
};
