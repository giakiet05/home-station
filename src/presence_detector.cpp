#include "presence_detector.h"

PresenceDetector::PresenceDetector()
    : bleScan_(nullptr),
      lastBleScanMs_(0),
      lastStrongBleMs_(0),
      lastPresenceTriggerMs_(0),
      prevLightAdc_(0),
      wasNightMode_(false),
      presenceDetected_(false),
      strongestRssi_(-120),
      scanStrongestRssi_(-120),
      state_(RoomPresenceState::AWAY),
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

    // 1. Detect light transitions
    bool lightTurnedOn = (wasNightMode_ && !isNightMode) ||
                         (prevLightAdc_ > 0 && currentLightAdc >= 800 && prevLightAdc_ < 800) ||
                         (prevLightAdc_ > 0 && currentLightAdc >= prevLightAdc_ + 400 && currentLightAdc >= 800);

    wasNightMode_ = isNightMode;
    prevLightAdc_ = currentLightAdc;

    // 2. Perform periodic 1-second passive BLE proximity scan every 10 seconds
    if (bleScan_ != nullptr && (now - lastBleScanMs_ >= 10000)) {
        lastBleScanMs_ = now;
        scanStrongestRssi_ = -120;
        
        bleScan_->start(1, false);
        strongestRssi_ = scanStrongestRssi_;
        bleScan_->clearResults();

        // Track when strong BLE was last seen in close proximity
        if (strongestRssi_ >= -65) {
            lastStrongBleMs_ = now;
        }
    }

    // 3. State Machine Transition & Trigger Evaluation
    if (state_ == RoomPresenceState::IN_ROOM) {
        // If no strong BLE seen for > 3 minutes AND room is dark -> Transition back to AWAY
        if ((now - lastStrongBleMs_ > 180000) && isNightMode) {
            state_ = RoomPresenceState::AWAY;
            Serial.println("[PRESENCE] Room state: IN_ROOM -> AWAY (User left or phone Bluetooth off)");
        }
    } else { // State is AWAY
        if (strongestRssi_ >= -60 && (now - lastPresenceTriggerMs_ >= 30000)) {
            state_ = RoomPresenceState::IN_ROOM;
            lastStrongBleMs_ = now;
            triggerPresence("BLE_PROXIMITY (Phone/Smartwatch nearby)");
        } else if (lightTurnedOn && (now - lastPresenceTriggerMs_ >= 15000)) {
            state_ = RoomPresenceState::IN_ROOM;
            triggerPresence("LIGHT_ON (Room light turned on)");
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


