#pragma once

#include <Arduino.h>

namespace setting_writer_internal {

struct SettingWriteRequest
{
  char key[4] = {0};
  int32_t targetValue = 0;
};

struct SettingWriterState
{
  bool running = false;
  uint8_t currentStepIndex = 0;
  uint32_t phaseStartedMs = 0;
  uint32_t invalidDisplayStartedMs = 0;
  bool stepStarted = false;
  bool keysReleased = false;
  int32_t currentValue = 0;
  bool increasing = false;
  bool adjustTimedOut = false;
  char previousDisplayText[9] = {0};
  char lastDisplayText[9] = {0};
  SettingWriteRequest request = {};
};

extern SettingWriterState g_state;
extern char g_lastCompletedKey[4];
extern uint32_t g_lastCompletedMs;
extern int32_t g_lastCompletedValue;

bool isValidSettingsStartDisplay(const char *displayText);
bool tryParseKey(const char *rawKey, char (&parsedKey)[4]);
void startStep(uint32_t now, uint32_t keyMask);
bool isWaitCompleted(uint32_t now, uint32_t ms);
void releaseKeys(uint32_t now);
void advanceToNextStep();
void finishWrite();
void abortWrite();
bool updateInvalidDisplayTimer(uint32_t now);

} // namespace setting_writer_internal