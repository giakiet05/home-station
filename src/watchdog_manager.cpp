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
      alertDispatchedForHang(false),
      pendingHangAlert(false),
      pendingRecoveryAlert(false) {}

#include <esp_wifi.h>

void WatchdogManager::init() {
    // 1. Wipe stale NVS WiFi cache to avoid corrupted AP metadata
    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    delay(200);

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Prevent RF modem sleep during handshake
    WiFi.setTxPower(WIFI_POWER_8_5dBm); // Critical for ESP32-C3 Super Mini voltage stability
    WiFi.setAutoReconnect(true);

    Serial.printf("[WATCHDOG] Connecting to SSID: '%s' (Pass length: %d) with 8.5dBm TX power\n",
                  Config::WIFI_SSID, strlen(Config::WIFI_PASSWORD));

    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);

    // Force HT20 20MHz bandwidth for RF stability
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);

    // Wait up to 15 seconds for initial connection
    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs < 15000)) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WATCHDOG] WiFi CONNECTED successfully! IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        sendTelegramAlert("[SYSTEM] Home Station ESP32 hardware watchdog active & connected to WiFi.");
    } else {
        Serial.printf("[WATCHDOG] Initial connection pending, status code: %d\n", WiFi.status());
    }
}

void WatchdogManager::feedHeartbeat() {
    lastHeartbeatMs = millis();
    if (!serverOnline) {
        serverOnline = true;
        hadInitialHeartbeat = true;
        pendingHangAlert = false;
        Serial.println("[WATCHDOG] Homeserver heartbeat established/restored.");

        if (alertDispatchedForHang) {
            alertDispatchedForHang = false;
            pendingRecoveryAlert = true;
        }
    }
}

bool WatchdogManager::isServerOnline() const {
    return serverOnline;
}

void WatchdogManager::ensureWiFiConnected() {
    uint32_t now = millis();
    if (now - lastWifiCheckMs >= 15000) {
        lastWifiCheckMs = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WATCHDOG] WiFi connection checking, attempting reconnect...");
            WiFi.reconnect();
        }
    }
}

bool WatchdogManager::sendTelegramAlert(const String& message) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WATCHDOG] WiFi not connected. Waiting up to 5s for connection...");
        if (WiFi.waitForConnectResult(5000) != WL_CONNECTED) {
            Serial.println("[WATCHDOG] Cannot send Telegram alert: WiFi connection unavailable.");
            return false;
        }
    }

    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate bundle verification for lightweight embedded TLS
    client.setTimeout(15000); // 15,000 milliseconds (15 seconds)

    HTTPClient http;
    http.setTimeout(15000); // 15,000 milliseconds (15 seconds)
    http.setReuse(false);
    String url = "https://api.telegram.org/bot" + String(Config::TELEGRAM_BOT_TOKEN) + "/sendMessage";

    if (!http.begin(client, url)) {
        Serial.println("[WATCHDOG] Failed to initialize HTTPS connection to Telegram API.");
        return false;
    }

    http.addHeader("Content-Type", "application/json");

    String escapedMsg = message;
    escapedMsg.replace("\"", "\\\"");
    escapedMsg.replace("\n", "\\n");

    // Send numeric chat_id
    String jsonPayload = "{\"chat_id\": " + String(Config::TELEGRAM_CHAT_ID) +
                         ", \"text\": \"" + escapedMsg + "\"}";

    int httpCode = http.POST(jsonPayload);
    bool success = (httpCode == 200);

    if (success) {
        Serial.println("[WATCHDOG] Telegram alert dispatched successfully.");
    } else {
        Serial.printf("[WATCHDOG] Telegram alert failed, HTTP code: %d, error: %s\n",
                      httpCode, http.errorToString(httpCode).c_str());
    }

    http.end();
    return success;
}

void WatchdogManager::update(bool smokeDetected) {
    ensureWiFiConnected();
    uint32_t now = millis();

    // 1. Check if heartbeat timeout has elapsed
    if (serverOnline && (now - lastHeartbeatMs >= Config::HEARTBEAT_TIMEOUT_MS)) {
        serverOnline = false;
        alertDispatchedForHang = true;
        pendingHangAlert = true;
        Serial.println("[WATCHDOG] Homeserver heartbeat lost > 25s! Triggering emergency alert.");
    }

    // 2. Dispatch pending hang alert
    if (pendingHangAlert && (WiFi.status() == WL_CONNECTED)) {
        if (sendTelegramAlert("[ALERT] Hardware Watchdog: Homeserver (HP 15) heartbeat lost (>25s). Possible system freeze or power down.")) {
            pendingHangAlert = false;
        }
    }

    // 3. Dispatch pending recovery alert
    if (pendingRecoveryAlert && (WiFi.status() == WL_CONNECTED)) {
        if (sendTelegramAlert("[RECOVERY] Hardware Watchdog: Homeserver (HP 15) serial connection and heartbeat restored.")) {
            pendingRecoveryAlert = false;
        }
    }

    // 4. Startup grace period check
    if (!hadInitialHeartbeat && (now > 35000) && !alertDispatchedForHang) {
        alertDispatchedForHang = true;
        Serial.println("[WATCHDOG] No initial heartbeat received after boot timeout.");
        sendTelegramAlert("[WARN] Hardware Watchdog: ESP32 started but no serial heartbeat received from Homeserver.");
    }

    // 5. Update LED visual state based on priority
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
