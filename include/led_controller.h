#pragma once

#include <Arduino.h>
#include <cstdint>

/**
 * @brief Enumeration of supported visual LED system states.
 */
enum class LedState : uint8_t {
    BOOTING,           // Blue LED
    SERVER_OK,         // Green LED
    SERVER_HANG,       // Red LED (Heartbeat lost / hang)
    GAS_DANGER         // Yellow / Orange LED (Gas or Smoke detected)
};

/**
 * @class LedController
 * @brief Manages 4-pin RGB LED (Common Cathode) and on-board diagnostic LED.
 */
class LedController {
public:
    LedController();

    /**
     * @brief Initializes GPIO pins for RGB LED and Builtin LED.
     */
    void init();

    /**
     * @brief Sets the current visual state for the RGB LED.
     * @param state The target state to render.
     */
    void setState(LedState state);

    /**
     * @brief Configures night mode state (turns off status LED when dark to prevent glare).
     * @param isNight True if ambient room lighting is dark.
     */
    void setNightMode(bool isNight);

    /**
     * @brief Periodic update routine to drive animations and heartbeat blinks.
     */
    void update();

private:
    LedState currentState;
    bool isNightMode;
    uint32_t lastHeartbeatBlinkMs;
    bool builtinLedActive;

    void applyColor(bool red, bool green, bool blue);
};
