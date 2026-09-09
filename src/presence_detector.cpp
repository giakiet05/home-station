#include "presence_detector.h"

PresenceDetector::PresenceDetector()
    : bleScan_(nullptr),
      lastBleScanMs_(0),
      lastPresenceTriggerMs_(0),
      prevLightAdc_(0),
      wasNightMode_(false),
      presenceDetected_(false),
      strongestRssi_(-120),
      scanStrongestRssi_(-120),
      triggerReason_("NONE") {}

void PresenceDetector::init() {
    NimBLEDevice::init("");
    bleScan_ = NimBLEDevice::getScan();
    bleScan_->setAdvertisedDeviceCallbacks(this, false);
    bleScan_->setActiveScan(false);
    bleScan_->setInterval(100);
    bleScan_->setWindow(99);
    Serial.println("[INFO] NimBLE presence proximity scanner initialized.");
}

void PresenceDetector::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    if (advertisedDevice == nullptr) return;
    int rssi = advertisedDevice->getRSSI();
    if (rssi > scanStrongestRssi_) {
        scanStrongestRssi_ = static_cast<int8_t>(rssi);
    }
}

void PresenceDetector::update(uint16_t currentLightAdc, bool isNightMode) {
    uint32_t now = millis();

    // 1. Detect abrupt light step jump (Dark -> Light = someone entered and flipped switch)
    if (wasNightMode_ && !isNightMode && (currentLightAdc >= 1200)) {
        if (now - lastPresenceTriggerMs_ >= 60000) { // 60s cooldown for light trigger
            triggerPresence("LIGHT_ON (Phong vua bat den)");
        }
    }
    wasNightMode_ = isNightMode;
    prevLightAdc_ = currentLightAdc;

    // 2. Perform periodic 1-second passive BLE proximity scan every 10 seconds
    if (bleScan_ != nullptr && (now - lastBleScanMs_ >= 10000)) {
        lastBleScanMs_ = now;
        scanStrongestRssi_ = -120;
        
        bleScan_->start(1, false);
        strongestRssi_ = scanStrongestRssi_;
        bleScan_->clearResults();

        // If strong BLE device is within close proximity (RSSI > -65 dBm) and cooldown elapsed
        if (strongestRssi_ > -65 && (now - lastPresenceTriggerMs_ >= 180000)) { // 3-minute cooldown
            triggerPresence("BLE_PROXIMITY (Dien thoai/Smartwatch toi gan)");
        }
    }
}

void PresenceDetector::triggerPresence(const char* reason) {
    presenceDetected_ = true;
    triggerReason_ = reason;
    lastPresenceTriggerMs_ = millis();
    Serial.printf("[PRESENCE] Room Entry Detected! Trigger: %s | Strongest BLE: %d dBm\n",
                  reason, strongestRssi_);
}

bool PresenceDetector::isPresenceDetected() const {
    return presenceDetected_;
}

int8_t PresenceDetector::getStrongestRssi() const {
    return strongestRssi_;
}

const char* PresenceDetector::getTriggerReason() const {
    return triggerReason_.c_str();
}

void PresenceDetector::clearPresence() {
    presenceDetected_ = false;
}

