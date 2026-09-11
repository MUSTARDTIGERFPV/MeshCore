#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>

// An ExpressLRS receiver rather than a transmitter, so almost nothing to do:
// no amplifier control, no fan, no backpack, and no battery divider (it is fed
// from the flight controller's 5 V rail). ESP32Board covers the rest, including
// the WS2812 via P_LORA_TX_NEOPIXEL_LED, and its getIRQGpio() already returns
// P_LORA_DIO_1 which is the right IRQ line for the LR11x0 family.
class DakefpvC3Board : public ESP32Board {
public:
  const char* getManufacturerName() const override {
    return "DAKEFPV C3 Gemini RX";
  }
};
