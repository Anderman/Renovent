#pragma once

#include <WString.h>

constexpr uint8_t kAutoUpdateLogCapacity = 6;

struct AutoUpdateStatus {
  String currentFirmwareBuildId;
  String currentSpiffsBuildId;
  String latestFirmwareBuildId;
  String latestSpiffsBuildId;
  String state;
  String lastError;
  unsigned long lastCheckMillis;
  unsigned long lastCheckDurationMs;
  unsigned long nextCheckMillis;
  uint32_t checkCount;
  uint32_t successfulCheckCount;
  String lastCheckResult;
  uint8_t logCount;
  String logEntries[kAutoUpdateLogCapacity];
  bool firmwareUpdateAvailable;
  bool spiffsUpdateAvailable;
  bool checkQueued;
};

void setupAutoUpdate();
void autoUpdateLoop();
void queueAutoUpdateCheck();
const AutoUpdateStatus &getAutoUpdateStatus();
String getCurrentFirmwareBuildId();
String getCurrentSpiffsBuildId();