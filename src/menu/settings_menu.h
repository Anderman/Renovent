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

struct SettingsMenuHaStatus {
  uint8_t count;
  uint32_t lastCompletedMs;
  SettingValue values[64];
};

using SettingsMenuValue = SettingValue;

void settingsMenuSetup();
void settingsMenuLoop();
bool requestSettingsMenuRead();
bool settingsMenuIsBusy();
void updateSettingsMenuValueFromWrite(const char *key, const char *rawValue);
SettingsMenuHaStatus getSettingsMenuHaStatus();
SettingsMenuStatus getSettingsMenuWebStatus();
bool getSettingsMenuWebValue(uint8_t index, SettingsMenuValue &value);