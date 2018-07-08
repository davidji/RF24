
#include "rf24.h"

const std::array<SettingValue, 4> RF24InternalSettings::AckPayloads::ENABLE = {
      feature.enable(),
      DynamicPayload::feature.enable(),
      DynamicPayload::pipe(0).enable(),
      DynamicPayload::pipe(1).enable()
};
    
const std::array<SettingValue, 4> RF24InternalSettings::INIT = {
    ConfigRegister::INIT,
    Retries::retries(5, 15),
    FeatureRegister::INIT,
    DynamicPayloadsRegister::INIT
};

const SettingValue RF24InternalSettings::Power::UP;
const SettingValue RF24InternalSettings::Power::DOWN;
const SettingValue RF24InternalSettings::PrimaryRx::ENABLE;

const SettingValue Power::MAX;
