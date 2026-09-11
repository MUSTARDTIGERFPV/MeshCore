#include <Arduino.h>
#include "target.h"
#include <helpers/UsbLogging.h>

DakefpvC3Board board;

// The board carries two LR1121s for receive diversity. MeshCore drives one
// radio, so the second (nss 7, busy 8, dio1 18, rst 10) is left unclaimed.
RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, SPI);
WRAPPER_CLASS radio_driver(radio, board);

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
SensorManager sensors;

#ifndef LORA_CR
  #define LORA_CR 5
#endif

// The RF switch is driven from the LR1121's own DIOs. This board's ExpressLRS
// layout declares no radio_rfsw_ctrl, which means ExpressLRS falls back to the
// default table in LR1121Driver::SetDioAsRfSwitch() - enable 0x0F over
// DIO5..DIO8, RX on DIO7, TX and TX_HP on DIO8, TX_HF on DIO6, WiFi on DIO5.
// RadioLib indexes its mode bits by position in the pin array, so listing the
// DIOs in ascending order reproduces the same bits.
static const uint32_t rfswitch_dios[Module::RFSWITCH_MAX_PINS] = {
  RADIOLIB_LR11X0_DIO5,
  RADIOLIB_LR11X0_DIO6,
  RADIOLIB_LR11X0_DIO7,
  RADIOLIB_LR11X0_DIO8,
  RADIOLIB_NC
};

static const Module::RfSwitchMode_t rfswitch_table[] = {
  // mode                 DIO5  DIO6  DIO7  DIO8
  { LR11x0::MODE_STBY,   {LOW,  LOW,  LOW,  LOW  }},
  { LR11x0::MODE_RX,     {LOW,  LOW,  HIGH, LOW  }},
  { LR11x0::MODE_TX,     {LOW,  LOW,  LOW,  HIGH }},
  { LR11x0::MODE_TX_HP,  {LOW,  LOW,  LOW,  HIGH }},
  { LR11x0::MODE_TX_HF,  {LOW,  HIGH, LOW,  LOW  }},
  { LR11x0::MODE_GNSS,   {LOW,  LOW,  LOW,  LOW  }},
  { LR11x0::MODE_WIFI,   {HIGH, LOW,  LOW,  LOW  }},
  END_OF_MODE_TABLE,
};

bool radio_init() {
  fallback_clock.begin();
  rtc_clock.begin(Wire);

  SPI.begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);

  // No TCXO. The ExpressLRS layout declares no radio_tcxo, so ExpressLRS never
  // issues SET_TCXO_MODE and the LR1121 runs from its 32 MHz crystal. Passing
  // zero makes RadioLib skip setTCXO() for the same reason - do not let this
  // fall back to a voltage, or DIO3 gets driven as a supply that isn't there.
  int status = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                           RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE,
                           LORA_TX_POWER, 16, LR11X0_DIO3_TCXO_VOLTAGE);
  if (status != RADIOLIB_ERR_NONE) {
    mesh::usbLoggingPort().printf("ERROR: radio init failed: %d\r\n", status);
    return false;
  }

  radio.setCRC(2);
  radio.explicitHeader();
  radio.setRfSwitchTable(rfswitch_dios, rfswitch_table);

  // Reuse this sequence if a runtime TX failure needs a hard radio recovery.
  radio_driver.setDeepInitCallback(radio_init);
  return true;
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);  // create new random identity
}
