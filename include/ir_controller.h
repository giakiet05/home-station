#pragma once

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_LG.h>
#include "config.h"

/**
 * @file ir_controller.h
 * @brief Manages Infrared transmission and protocol encoding for LG Air Conditioners.
 */

/**
 * @class IrController
 * @brief Controls the LG AC via hardware IR LED on ESP32-C3.
 */
class IrController {
public:
    /**
     * @brief Constructs an IrController bound to a specified GPIO pin.
     * @param pin The GPIO pin connected to the IR LED transmitter (defaults to Config::PIN_IR_TRANSMITTER).
     */
    explicit IrController(uint8_t pin = Config::PIN_IR_TRANSMITTER);

    /**
     * @brief Initializes the underlying IR hardware timer and loads default AC state.
     */
    void init();

    /**
     * @brief Sets power state and sends the IR frame.
     * @param power True to power on, false to power off.
     */
    void setPower(bool power);

    /**
     * @brief Sets target cooling temperature and sends the IR frame.
     * @param temp Target temperature in Celsius (clamped between 16 and 30).
     */
    void setTemp(uint8_t temp);

    /**
     * @brief Sets full AC state and immediately transmits the IR packet.
     * @param power True for ON, false for OFF.
     * @param temp Target temperature in Celsius.
     * @param mode Operating mode (e.g., "cool", "fan", "dry").
     * @param fan Fan speed (e.g., "auto", "low", "med", "high").
     */
    void sendCommand(bool power, uint8_t temp = 26, const String &mode = "cool", const String &fan = "auto");

    /**
     * @brief Parses and processes incoming textual command frames (from Serial or network).
     * @param rawCommand Raw command string (e.g., "AC:ON,26", "AC:OFF", or JSON payload).
     * @return true if command was recognized and handled, false otherwise.
     */
    bool handleCommand(const String &rawCommand);

    /**
     * @brief Retrieves the current cached target temperature.
     * @return uint8_t Target temperature in Celsius.
     */
    uint8_t getTemp() const;

    /**
     * @brief Retrieves the current cached power state.
     * @return bool True if AC state is ON, false if OFF.
     */
    bool getPower() const;

private:
    IRLgAc ac_;
    uint8_t current_temp_;
    bool is_power_on_;
};
