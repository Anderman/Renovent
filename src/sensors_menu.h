#pragma once

#include <Arduino.h>

struct SensorsMenuCapturedEntry {
  bool available;
  char rawValue[9];
  bool hasValue;
  int32_t value;
  bool hasAuxValue;
  int32_t auxValue;
};

struct SensorsMenuValueItem {
  bool available;
  bool hasValue;
  int32_t value;
};

struct SensorsMenuUnknownEntry {
  bool available;
  char key[4];
  char rawValue[9];
  bool hasValue;
  int32_t value;
};

struct SensorsMenuDefinition {
  const char *example;
  const char *description;
  const char *remark;
};

struct SensorsMenuValueDefinition {
  const char *key;
  const char *description;
  const char *unit;
  const char *remark;
  uint8_t displayPrecision;
};

struct SensorsMenuProgress {
  bool running;
  uint8_t currentStep;
  uint32_t lastCompletedMs;
  char phase[24];
  char lastDisplayText[9];
};

struct SensorsMenuSnapshot {
  uint32_t lastCompletedMs;
  SensorsMenuCapturedEntry entries[16];
  SensorsMenuValueItem values[17];
  SensorsMenuUnknownEntry unknownEntries[8];
};

void sensorsMenuSetup();
void sensorsMenuLoop();
void startSensorsMenuScan();
bool canStartSensorsMenu();
bool sensorsMenuIsBusy();
SensorsMenuProgress getSensorsMenuProgress();
SensorsMenuSnapshot getSensorsMenuSnapshot();
SensorsMenuDefinition getSensorsMenuDefinition(uint8_t step);
SensorsMenuValueDefinition getSensorsMenuValueDefinition(uint8_t index);