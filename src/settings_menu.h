#pragma once

#include <Arduino.h>

struct SettingValue {
  bool available;
  char key[4];
  char rawValue[9];
  bool hasValue;
  int32_t value;
};

struct SettingsMenuStatus {
  bool running;
  uint8_t count;
  uint32_t lastCompletedMs;
  char phase[24];
  char lastDisplayText[9];
};

using SettingsMenuValue = SettingValue;

void settingsMenuSetup();
void settingsMenuLoop();
bool requestSettingsMenuRead();
bool settingsMenuIsBusy();
SettingsMenuStatus getSettingsMenuStatus();
bool getSettingsMenuValue(uint8_t index, SettingsMenuValue &value);