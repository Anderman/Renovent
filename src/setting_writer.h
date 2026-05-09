#pragma once

#include <Arduino.h>

enum class SettingWriteStatus : uint8_t {
  Scheduled,
  Busy,
  InvalidKey,
  InvalidStartDisplay,
};

struct SettingWriterStatus {
  bool running;
  char key[4];
  int32_t value;
  uint32_t lastCompletedMs;
};

void settingWriterSetup();
void settingWriterLoop();
SettingWriteStatus writeSetting(const char *key, int32_t value);
bool settingWriterIsBusy();
SettingWriterStatus getSettingWriterStatus();