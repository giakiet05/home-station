#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "watchdog_manager.h"
#include "config.h"

WatchdogManager::WatchdogManager(LedController& ledCtrl)
    : ledController(ledCtrl),
      lastHeartbeatMs(0),
      lastWifiCheckMs(0),
      serverOnline(false),
      hadInitialHeartbeat(false),
      alertDispatchedForHang(false) {}

void WatchdogManager::init() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
    Serial.println("[WATCHDOG] Initializing WiFi connection to SSID: " + String(Config::WIFI_SSID));
}

void WatchdogManager::feedHeartbeat() {
    lastHeartbeatMs = millis();
    if (!serverOnline) {
        serverOnline = true;
        hadInitialHeartbeat = true;
        Serial.println("[WATCHDOG] Homeserver heartbeat established/restored.");

        if (alertDispatchedForHang) {
            alertDispatchedForHang = false;
            sendTelegramAlert("[RECOVERY] Hardware Watchdog: Homeserver (HP 15) serial connection and heartbeat restored.");
        }
    }
}

bool WatchdogManager::isServerOnline() const {
    return serverOnline;
}

void WatchdogManager::ensureWiFiConnected() {
    uint32_t now = millis();
    if (now - lastWifiCheckMs >= 10000) {
        lastWifiCheckMs = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WATCHDOG] WiFi not connected, attempting connection...");
            WiFi.disconnect();
            WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
        }
    }
}

bool WatchdogManager::sendTelegramAlert(const String& message) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WATCHDOG] Cannot send Telegram alert: WiFi not connected.");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate bundle validation for lightweight TLS on ESP32

    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(Config::TELEGRAM_BOT_TOKEN) + "/sendMessage";

    if (!http.begin(client, url)) {
        Serial.println("[WATCHDOG] Failed to initialize HTTPS connection to Telegram API.");
        return false;
    }

    http.addHeader("Content-Type", "application/json");

    String escapedMsg = message;
    escapedMsg.replace("\"", "\\\"");
    escapedMsg.replace("\n", "\\n");

    String jsonPayload = "{\"chat_id\":\"" + String(Config::TELEGRAM_CHAT_ID) +
                         "\",\"text\":\"" + escapedMsg +
                         "\"}";

    int httpCode = http.POST(jsonPayload);
    bool success = (httpCode == 200);

    if (success) {
        Serial.println("[WATCHDOG] Telegram alert dispatched successfully.");
    } else {
        Serial.println("[WATCHDOG] Telegram alert failed, HTTP code: " + String(httpCode));
    }

    http.end();
    return success;
}

void WatchdogManager::update(bool smokeDetected) {
    ensureWiFiConnected();
    uint32_t now = millis();

    // Check if heartbeat timeout has elapsed
    if (serverOnline && (now - lastHeartbeatMs >= Config::HEARTBEAT_TIMEOUT_MS)) {
        serverOnline = false;
        alertDispatchedForHang = true;
        Serial.println("[WATCHDOG] Homeserver heartbeat lost > 25s! Triggering emergency alert.");
        sendTelegramAlert("[ALERT] Hardware Watchdog: Homeserver (HP 15) heartbeat lost (>25s). Possible system freeze or power down.");
    }

    // Startup grace period check
    if (!hadInitialHeartbeat && (now > 35000) && !alertDispatchedForHang) {
        alertDispatchedForHang = true;
        Serial.println("[WATCHDOG] No initial heartbeat received after boot timeout.");
        sendTelegramAlert("[WARN] Hardware Watchdog: ESP32 started but no serial heartbeat received from Homeserver.");
    }

    // Update LED visual state based on priority
    if (smokeDetected) {
        ledController.setState(LedState::GAS_DANGER);
    } else if (!serverOnline && (hadInitialHeartbeat || now > 35000)) {
        ledController.setState(LedState::SERVER_HANG);
    } else if (serverOnline) {
        ledController.setState(LedState::SERVER_OK);
    } else {
        ledController.setState(LedState::BOOTING);
    }
}
