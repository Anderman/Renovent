#pragma once

#include <Arduino.h>

struct SensorsMenuCapturedEntry {
  bool available;
  char rawValue[9];
  char detail[32];
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
};

struct SensorsMenuStatus {
  bool running;
  bool done;
  uint8_t currentStep;
  uint32_t lastCompletedMs;
  char phase[24];
  char lastDisplayText[9];
  SensorsMenuCapturedEntry entries[13];
  SensorsMenuValueItem values[14];
  SensorsMenuUnknownEntry unknownEntries[8];
};

void sensorsMenuSetup();
void sensorsMenuLoop();
void startSensorsMenuScan();
void stopSensorsMenuScan();
bool sensorsMenuIsBusy();
bool sensorsMenuAutoScanEnabled();
SensorsMenuStatus getSensorsMenuStatus();
SensorsMenuDefinition getSensorsMenuDefinition(uint8_t step);
SensorsMenuValueDefinition getSensorsMenuValueDefinition(uint8_t index);