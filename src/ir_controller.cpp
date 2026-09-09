#include "ir_controller.h"

/**
 * @brief Constructs an IrController instance.
 * @param pin The GPIO pin driving the IR transmitter LED.
 */
IrController::IrController(uint8_t pin)
    : ac_(pin),
      current_temp_(26),
      is_power_on_(false) {}

/**
 * @brief Initializes the IRLgAc driver for LG Dual Inverter protocol (LG2 / AKB75075801 compatible).
 */
void IrController::init() {
    ac_.begin();
    ac_.setModel(lg_ac_remote_model_t::AKB75215403);
    ac_.setPower(false);
    ac_.setMode(kLgAcCool);
    ac_.setTemp(current_temp_);
    ac_.setFan(kLgAcFanAuto);
    ac_.setSwingV(kLgAcSwingVOff);

    Serial.printf("[INFO] IR Controller initialized on GPIO%d for LG Dual Inverter (AKB75075801 / AKB75215403).\n", Config::PIN_IR_TRANSMITTER);
}

/**
 * @brief Sets power state and transmits IR packet.
 * @param power True for ON, false for OFF.
 */
void IrController::setPower(bool power) {
    is_power_on_ = power;
    ac_.setPower(power);
    ac_.send(1);

    Serial.printf("[IR] Sent LG Dual Inverter Power %s command.\n", power ? "ON" : "OFF");
}

/**
 * @brief Sets target temperature and transmits IR packet.
 * @param temp Target temperature in Celsius (16 - 30).
 */
void IrController::setTemp(uint8_t temp) {
    if (temp < 16) temp = 16;
    if (temp > 30) temp = 30;

    current_temp_ = temp;
    ac_.setTemp(current_temp_);
    ac_.send(1);

    Serial.printf("[IR] Sent LG Dual Inverter Temp command: %d C.\n", current_temp_);
}

/**
 * @brief Sets full AC parameters and transmits the IR packet.
 * @param power Power state.
 * @param temp Target temperature.
 * @param mode Operating mode string ("cool", "dry", "fan", "heat", "auto").
 * @param fan Fan speed string ("auto", "low", "medium", "high", "max").
 */
void IrController::sendCommand(bool power, uint8_t temp, const String &mode, const String &fan) {
    if (temp < 16) temp = 16;
    if (temp > 30) temp = 30;

    is_power_on_ = power;
    current_temp_ = temp;

    ac_.setPower(power);
    ac_.setTemp(temp);

    // Set mode
    String modeLower = mode;
    modeLower.toLowerCase();
    if (modeLower == "dry") {
        ac_.setMode(kLgAcDry);
    } else if (modeLower == "fan") {
        ac_.setMode(kLgAcFan);
    } else if (modeLower == "heat") {
        ac_.setMode(kLgAcHeat);
    } else if (modeLower == "auto") {
        ac_.setMode(kLgAcAuto);
    } else {
        ac_.setMode(kLgAcCool);
    }

    // Set fan speed
    String fanLower = fan;
    fanLower.toLowerCase();
    if (fanLower == "low") {
        ac_.setFan(kLgAcFanLow);
    } else if (fanLower == "med" || fanLower == "medium") {
        ac_.setFan(kLgAcFanMedium);
    } else if (fanLower == "high") {
        ac_.setFan(kLgAcFanHigh);
    } else if (fanLower == "max") {
        ac_.setFan(kLgAcFanMax);
    } else {
        ac_.setFan(kLgAcFanAuto);
    }

    ac_.send(1);

    Serial.printf("[IR] Dispatched LG Dual Inverter frame: Power=%s, Temp=%dC, Mode=%s, Fan=%s\n",
                  power ? "ON" : "OFF",
                  temp,
                  mode.c_str(),
                  fan.c_str());
}

/**
 * @brief Parses incoming textual command lines.
 * Supported formats:
 * - "AC:ON,26" or "AC:ON"
 * - "AC:OFF"
 * - "AC:TEMP,25"
 * - "AC:TEST" (sweeps across all LG2 model variants)
 * - "{\"type\":\"ac\",\"power\":true,\"temp\":25}"
 * @param rawCommand Incoming raw string.
 * @return true if handled, false otherwise.
 */
bool IrController::handleCommand(const String &rawCommand) {
    String cmd = rawCommand;
    cmd.trim();

    if (cmd.length() == 0) {
        return false;
    }

    // Format 1: Text command prefix "AC:"
    if (cmd.startsWith("AC:") || cmd.startsWith("ac:")) {
        String payload = cmd.substring(3);
        payload.trim();

        if (payload.equalsIgnoreCase("OFF")) {
            setPower(false);
            return true;
        }

        if (payload.equalsIgnoreCase("CHECK") || payload.equalsIgnoreCase("DIAG")) {
            Serial.println("\n[DIAG] ==================================================");
            Serial.println("[DIAG] STARTING HARDWARE PIN & LED DIAGNOSIS ON GPIO3...");
            pinMode(Config::PIN_IR_TRANSMITTER, INPUT_PULLUP);
            delay(50);

            uint32_t adcSum = 0;
            for (int i = 0; i < 10; i++) {
                adcSum += analogRead(Config::PIN_IR_TRANSMITTER);
                delay(20);
            }
            uint16_t adcRaw = adcSum / 10;
            float voltage = (static_cast<float>(adcRaw) / 4095.0f) * 3.3f;

            Serial.printf("[DIAG] GPIO3 Measured Voltage = %.2f V (ADC Raw = %u / 4095)\n", voltage, adcRaw);

            if (voltage >= 2.7f) {
                Serial.println("[DIAG] EVALUATION: OPEN CIRCUIT or REVERSE BIAS.");
                Serial.println("[DIAG] -> Khong co dong chay qua LED xuong GND.");
                Serial.println("[DIAG] -> Nguyen nhan: Cam sai chan GPIO3 / Cam nguoc cuc LED / Long day.");
            } else if (voltage >= 0.7f && voltage <= 2.2f) {
                Serial.printf("[DIAG] EVALUATION: LED DETECTED (Forward Drop = %.2fV).\n", voltage);
                Serial.println("[DIAG] -> Chuc mung: LED da cam DUNG CUC va mach kin!");
            } else {
                Serial.println("[DIAG] EVALUATION: DIRECT SHORT TO GND.");
                Serial.println("[DIAG] -> Chan GPIO3 bi noi thang xuong GND khong qua LED.");
            }
            Serial.println("[DIAG] ==================================================\n");

            ac_.begin();
            return true;
        }

        if (payload.equalsIgnoreCase("FLASH")) {
            Serial.println("[IR] Holding GPIO3 HIGH for 1s on / 0.5s off (5 cycles) for camera check...");
            pinMode(Config::PIN_IR_TRANSMITTER, OUTPUT);
            for (int i = 0; i < 5; i++) {
                digitalWrite(Config::PIN_IR_TRANSMITTER, HIGH);
                delay(1000);
                digitalWrite(Config::PIN_IR_TRANSMITTER, LOW);
                delay(500);
            }
            ac_.begin(); // Re-init IRLgAc
            Serial.println("[IR] Hardware Flash Test completed.");
            return true;
        }

        if (payload.equalsIgnoreCase("ALL") || payload.equalsIgnoreCase("TEST")) {
            Serial.println("\n[IR] ==================================================");
            Serial.println("[IR] RUNNING EXHAUSTIVE LG & LG2 PROTOCOL SWEEP...");

            // 1. AKB75215403 (LG2 Protocol - Standard Dual Inverter)
            ac_.setModel(lg_ac_remote_model_t::AKB75215403);
            ac_.setPower(true);
            ac_.setMode(kLgAcCool);
            ac_.setTemp(26);
            ac_.setFan(kLgAcFanAuto);
            ac_.send(0);
            ac_.send(1);
            Serial.println("[IR] [1/5] Sent AKB75215403 (LG2 28-bit)");
            delay(1000);

            // 2. AKB74955603 (LG2 Variant)
            ac_.setModel(lg_ac_remote_model_t::AKB74955603);
            ac_.setPower(true);
            ac_.setMode(kLgAcCool);
            ac_.setTemp(26);
            ac_.setFan(kLgAcFanAuto);
            ac_.send(0);
            ac_.send(1);
            Serial.println("[IR] [2/5] Sent AKB74955603 (LG2 Variant)");
            delay(1000);

            // 3. AKB73757604 (LG2 Variant)
            ac_.setModel(lg_ac_remote_model_t::AKB73757604);
            ac_.setPower(true);
            ac_.setMode(kLgAcCool);
            ac_.setTemp(26);
            ac_.setFan(kLgAcFanAuto);
            ac_.send(0);
            ac_.send(1);
            Serial.println("[IR] [3/5] Sent AKB73757604 (LG2 Variant)");
            delay(1000);

            // 4. GE6711AR2853M (LG Protocol v1)
            ac_.setModel(lg_ac_remote_model_t::GE6711AR2853M);
            ac_.setPower(true);
            ac_.setMode(kLgAcCool);
            ac_.setTemp(26);
            ac_.setFan(kLgAcFanAuto);
            ac_.send(0);
            ac_.send(1);
            Serial.println("[IR] [4/5] Sent GE6711AR2853M (LG v1 28-bit)");
            delay(1000);

            // 5. LG6711A20083V (LG Protocol v1 Variant)
            ac_.setModel(lg_ac_remote_model_t::LG6711A20083V);
            ac_.setPower(true);
            ac_.setMode(kLgAcCool);
            ac_.setTemp(26);
            ac_.setFan(kLgAcFanAuto);
            ac_.send(0);
            ac_.send(1);
            Serial.println("[IR] [5/5] Sent LG6711A20083V (LG v1 Variant)");

            // Restore AKB75215403
            ac_.setModel(lg_ac_remote_model_t::AKB75215403);
            Serial.println("[IR] ==================================================\n");
            return true;
        }

        if (payload.startsWith("ON") || payload.startsWith("on")) {
            uint8_t temp = 26;
            int commaIdx = payload.indexOf(',');
            if (commaIdx > 0) {
                int parsed = payload.substring(commaIdx + 1).toInt();
                if (parsed >= 16 && parsed <= 30) {
                    temp = static_cast<uint8_t>(parsed);
                }
            }
            sendCommand(true, temp, "cool", "auto");
            return true;
        }

        if (payload.startsWith("TEMP,") || payload.startsWith("temp,")) {
            int parsed = payload.substring(5).toInt();
            if (parsed >= 16 && parsed <= 30) {
                setTemp(static_cast<uint8_t>(parsed));
                return true;
            }
        }
    }

    // Format 2: JSON payload
    if (cmd.startsWith("{") && cmd.endsWith("}")) {
        if (cmd.indexOf("\"type\":\"ac\"") >= 0 || cmd.indexOf("\"cmd\":\"ac\"") >= 0) {
            bool power = (cmd.indexOf("\"power\":true") >= 0 || cmd.indexOf("\"power\": 1") >= 0);
            uint8_t temp = current_temp_;

            int tempIdx = cmd.indexOf("\"temp\":");
            if (tempIdx >= 0) {
                int val = cmd.substring(tempIdx + 7).toInt();
                if (val >= 16 && val <= 30) {
                    temp = static_cast<uint8_t>(val);
                }
            }

            String mode = "cool";
            if (cmd.indexOf("\"dry\"") >= 0) mode = "dry";
            else if (cmd.indexOf("\"fan\"") >= 0) mode = "fan";

            sendCommand(power, temp, mode, "auto");
            return true;
        }
    }

    return false;
}

/**
 * @brief Retrieves the target temperature.
 * @return uint8_t Temperature in Celsius.
 */
uint8_t IrController::getTemp() const {
    return current_temp_;
}

/**
 * @brief Retrieves the power state.
 * @return bool True if ON, false if OFF.
 */
bool IrController::getPower() const {
    return is_power_on_;
}
