#pragma once

#include "CustomSX1276.h"
#include "RadioLibWrappers.h"

#ifndef USE_SX1276
#define USE_SX1276
#endif

class CustomSX1276Wrapper : public RadioLibWrapper {
public:
  CustomSX1276Wrapper(CustomSX1276& radio, mesh::MainBoard& board) : RadioLibWrapper(radio, board) { }

  void powerOff() { _radio->standby(); _radio->sleep(); }

protected:
  bool applyParams(float freq, float bw, uint8_t sf, uint8_t cr) override {
    return ((CustomSX1276 *)_radio)->setFrequency(freq) == RADIOLIB_ERR_NONE
        && ((CustomSX1276 *)_radio)->setSpreadingFactor(sf) == RADIOLIB_ERR_NONE
        && ((CustomSX1276 *)_radio)->setBandwidth(bw) == RADIOLIB_ERR_NONE
        && ((CustomSX1276 *)_radio)->setCodingRate(cr) == RADIOLIB_ERR_NONE
        && updatePreamble(sf, bw);
  }

public:
  bool setCodingRate(uint8_t cr) override {
    return ((CustomSX1276 *)_radio)->setCodingRate(cr) == RADIOLIB_ERR_NONE;
  }

  bool isReceivingPacket() override { 
    return ((CustomSX1276 *)_radio)->isReceiving();
  }
  float getCurrentRSSI() override {
    return ((CustomSX1276 *)_radio)->getRSSI(false);
  }
  float packetScore(float snr, int packet_len) override {
    int sf = ((CustomSX1276 *)_radio)->spreadingFactor;
    return packetScoreInt(snr, sf, packet_len);
  }
  uint8_t getSpreadingFactor() const override { return ((CustomSX1276 *)_radio)->spreadingFactor; }

protected:
  int16_t performChannelScan() override {
    return ((CustomSX1276 *)_radio)->tryScanChannel(cadScanTimeoutMillis(), *_board);
  }

  // Power handed to the FSK modem while keying a carrier. A board whose radio
  // only drives an external amplifier overrides this: giving beginFSK() the
  // user-facing dBm would push the PA far past the input it expects.
  virtual int8_t carrierDriveDbm() const { return LORA_TX_POWER; }

  // SX127x cannot emit a carrier from the LoRa modem - RadioLib's
  // transmitDirect() returns RADIOLIB_ERR_WRONG_MODEM there. So CW means
  // switching to FSK, and leaving it means rebuilding the LoRa config.
  int16_t enterCarrierWave() override {
    CustomSX1276* radio = (CustomSX1276 *)_radio;
    const float freq = _params_valid ? _cur_freq : (float)LORA_FREQ;
    const int16_t status = radio->beginFSK(freq, 4.8, 5.0, 125.0,
                                           carrierDriveDbm(), 16, false);
    if (status != RADIOLIB_ERR_NONE) return status;
    return radio->transmitDirect();
  }

  int16_t exitCarrierWave() override {
    _radio->standby();
    if (!radioDeepInit()) return RADIOLIB_ERR_UNKNOWN;
    return restoreAfterDeepInit() ? RADIOLIB_ERR_NONE : RADIOLIB_ERR_UNKNOWN;
  }

  bool radioDeepInit() override {
    if (!prepareRadioHardReset()) return false;
    return ((CustomSX1276 *)_radio)->std_init() && _board->finishRadioHardReset();
  }
  bool supportsRadioDeepInit() const override { return supportsRadioHardResetPath(); }
};
