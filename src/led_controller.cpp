#include "led_controller.h"
#include "config.h"

LedController::LedController()
    : currentState(LedState::BOOTING),
      isNightMode(false),
      lastHeartbeatBlinkMs(0),
      builtinLedActive(false) {}

void LedController::init() {
    pinMode(Config::PIN_RGB_RED, OUTPUT);
    pinMode(Config::PIN_RGB_GREEN, OUTPUT);
    pinMode(Config::PIN_RGB_BLUE, OUTPUT);
    pinMode(Config::PIN_BUILTIN_LED, OUTPUT);

    // Startup self-test: flash Red -> Green -> Blue for 300ms each to verify all 3 channels
    applyColor(true, false, false); // Red
    delay(300);
    applyColor(false, true, false); // Green
    delay(300);
    applyColor(false, false, true); // Blue
    delay(300);

    // Initial state: Blue (booting / initializing)
    setState(LedState::BOOTING);
    // Turn off builtin LED (Active LOW -> HIGH is OFF)
    digitalWrite(Config::PIN_BUILTIN_LED, HIGH);
}

void LedController::applyColor(bool red, bool green, bool blue) {
    // Common Cathode: HIGH turns on the LED channel, LOW turns it off
    digitalWrite(Config::PIN_RGB_RED, red ? HIGH : LOW);
    digitalWrite(Config::PIN_RGB_GREEN, green ? HIGH : LOW);
    digitalWrite(Config::PIN_RGB_BLUE, blue ? HIGH : LOW);
}

void LedController::setNightMode(bool isNight) {
    if (isNightMode != isNight) {
        isNightMode = isNight;
        setState(currentState);
    }
}

void LedController::setState(LedState state) {
    currentState = state;

    // In Night Mode, turn off all status LEDs to keep room dark (except GAS_DANGER which is an emergency)
    if (isNightMode && (currentState != LedState::GAS_DANGER)) {
        applyColor(false, false, false); // Completely OFF
        return;
    }

    switch (currentState) {
        case LedState::BOOTING:
            // Blue
            applyColor(false, false, true);
            break;
        case LedState::SERVER_OK:
            // Green
            applyColor(false, true, false);
            break;
        case LedState::SERVER_HANG:
            // Red (Critical - server freeze/offline)
            applyColor(true, false, false);
            break;
        case LedState::GAS_DANGER:
            // Yellow / Orange (Emergency - always shown even at night)
            applyColor(true, true, false);
            break;
    }
}

void LedController::update() {
    uint32_t now = millis();

    // In night mode, keep the onboard LED off as well
    if (isNightMode) {
        if (builtinLedActive) {
            builtinLedActive = false;
            digitalWrite(Config::PIN_BUILTIN_LED, HIGH); // OFF
        }
        return;
    }

    // Blink built-in LED (GPIO8) every 2 seconds as a heartbeat indicator
    if (now - lastHeartbeatBlinkMs >= 2000) {
        lastHeartbeatBlinkMs = now;
        builtinLedActive = true;
        digitalWrite(Config::PIN_BUILTIN_LED, LOW); // ON
    } else if (builtinLedActive && (now - lastHeartbeatBlinkMs >= 80)) {
        builtinLedActive = false;
        digitalWrite(Config::PIN_BUILTIN_LED, HIGH); // OFF
    }
}
