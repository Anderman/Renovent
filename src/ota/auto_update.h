#pragma once

#include <WString.h>

struct AutoUpdateStatus {
  String currentFirmwareBuildId;
  String currentSpiffsBuildId;
  String latestFirmwareBuildId;
  String latestSpiffsBuildId;
  String state;
  String lastError;
  unsigned long lastCheckMillis;
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