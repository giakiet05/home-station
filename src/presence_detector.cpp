#include "presence_detector.h"

PresenceDetector::PresenceDetector()
    : bleScan_(nullptr),
      lastBleScanMs_(0),
      lastLightTriggerMs_(0),
      lastBleTriggerMs_(0),
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

    // 1. Detect abrupt light step jump (Dark -> Light or sharp +400 ADC rise)
    bool lightJumped = (wasNightMode_ && !isNightMode) ||
                       (prevLightAdc_ > 0 && currentLightAdc >= 800 && prevLightAdc_ < 800) ||
                       (prevLightAdc_ > 0 && currentLightAdc >= prevLightAdc_ + 400 && currentLightAdc >= 800);

    if (lightJumped && (now - lastLightTriggerMs_ >= 15000)) { // 15s cooldown for light flip
        lastLightTriggerMs_ = now;
        triggerPresence("LIGHT_ON (Phong vua bat den)");
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

        // If strong BLE device is within close proximity (RSSI > -60 dBm) and cooldown elapsed
        if (strongestRssi_ > -60 && (now - lastBleTriggerMs_ >= 120000)) { // 2-minute cooldown
            lastBleTriggerMs_ = now;
            triggerPresence("BLE_PROXIMITY (Dien thoai/Smartwatch toi gan)");
        }
    }
}

void PresenceDetector::triggerPresence(const char* reason) {
    presenceDetected_ = true;
    triggerReason_ = reason;
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

