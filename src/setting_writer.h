#pragma once

#include <Arduino.h>

enum class SettingWriteRejectReason : uint8_t {
  None,
  Busy,
  InvalidKey,
  InvalidStartDisplay,
};

struct SettingWriteResult {
  bool scheduled;
  SettingWriteRejectReason rejectReason;
  char key[4];
  int32_t targetValue;
  char displayText[9];
};

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
SettingWriteResult requestSettingWriteDetailed(const char *key, int32_t value);
bool requestSettingWrite(const char *key, int32_t value);
bool settingWriterIsBusy();
SettingWriterStatus getSettingWriterStatus();