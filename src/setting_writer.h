#pragma once

#include <Arduino.h>

struct SettingWriterStatus {
  bool running;
  char key[4];
  int32_t currentValue;
  int32_t targetValue;
  uint32_t lastCompletedMs;
  char phase[24];
  char lastDisplayText[9];
};

void settingWriterSetup();
void settingWriterLoop();
bool requestSettingWrite(const char *key, int32_t value);
bool settingWriterIsBusy();
SettingWriterStatus getSettingWriterStatus();