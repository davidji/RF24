
#ifndef __RF24_SETTINGS_H__
#define __RF24_SETTINGS_H__

#include "rf24-config.h"
#include "nRF24L01.h"

/**
 * A setting is a value for a bit field in a register
 * Use it in set(Setting).
 */
typedef struct {
  const uint8_t reg;
  const uint8_t mask;
} Setting;

typedef struct {
  const Setting setting;
  const uint8_t value;
} SettingValue;

class BooleanSetting {
public:
  const Setting setting;
  constexpr SettingValue enable() {
    return { setting,setting.mask};
  }
  constexpr SettingValue disable() {
    return {setting, 0x00};
  }
};

class DataRate;

/**
 * The radio modem bitrate.
 *
 * For use with set(DataRate)
 */
class DataRateOption {
  const uint8_t value;

  constexpr DataRateOption(uint8_t value, uint32_t delay) 
    : value(value), txRxDelay(delay) {}

  friend DataRate;
public:
  const uint32_t txRxDelay;
  static constexpr Setting setting = { 
    RF_SETUP, _BV(RF_DR_LOW) | _BV(RF_DR_HIGH) };

  explicit constexpr operator SettingValue () {
    return { setting, value };
  }
};

class DataRate {
public:
  static constexpr auto _1MBPS = DataRateOption(0, RF24_1MBPS_TX_RX_DELAY);
  static constexpr auto _250KBPS = DataRateOption(_BV(RF_DR_LOW), RF24_250KBPS_TX_RX_DELAY);
  static constexpr auto _2MBPS = DataRateOption(_BV(RF_DR_HIGH), RF24_2MBPS_TX_RX_DELAY);
};

class CyclicRedundancyCheck {
public:
  static constexpr Setting setting = { CONFIG, _BV(CRCO) | _BV(EN_CRC) };
  static constexpr SettingValue DISABLED = { setting, 0 };
  static constexpr SettingValue CRC_8 = { setting, _BV(EN_CRC) };
  static constexpr SettingValue CRC_16 = { setting, _BV(EN_CRC) | _BV( CRCO )};
};

class DynamicPayload {
public:
  static constexpr BooleanSetting feature = { { FEATURE, _BV(EN_DPL) } };
  static constexpr BooleanSetting pipe(uint8_t channel) {
    return { { DYNPD, _BV(channel) } };
  }

  static constexpr BooleanSetting all = { { DYNPD, 0b111111 } };
};

class Power {
public:
  static constexpr Setting setting = { RF_SETUP, 0b110 };
  static constexpr SettingValue MINUS18DBM = { setting, 0b000 };
  static constexpr SettingValue MINUS12DBM = { setting, 0b010 };
  static constexpr SettingValue MINUS6DBM = { setting, 0b100 };
  static constexpr SettingValue MINUS0DBM = { setting, 0b110 };
  static constexpr auto MIN = MINUS18DBM;
  static constexpr auto MAX = MINUS0DBM;
};


class Retries {
public:
  static constexpr Setting setting = { SETUP_RETR, 0xff};
  static constexpr SettingValue retries(uint8_t delay, uint8_t count) {
    return { setting, (uint8_t)((delay&0xf)<<ARD | (count&0xf)<<ARC) };
  }
};

class AutoAck {
public:
  static constexpr BooleanSetting pipe(uint8_t p) {
    return BooleanSetting({ EN_AA, _BV(p) });
  }

  static constexpr BooleanSetting all = { { EN_AA, 0b111111 } };
};

class Channel {
public:
  static constexpr uint8_t max = 127;
  static constexpr Setting setting = { RF_CH, 0xff};
  static constexpr SettingValue channel(uint8_t channel) {
    return { setting, rf24_min(channel,max) };
  }
};

#endif // __RF24_SETTINGS_H__