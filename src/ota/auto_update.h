#pragma once

#include <WString.h>

struct AutoUpdateStatus {
  String currentFirmwareBuildId;
  String currentSpiffsBuildId;
  String state;
  unsigned long lastCheckMillis;
  unsigned long nextCheckMillis;
  String lastCheckResult;
};

void setupAutoUpdate();
void autoUpdateLoop();
void queueAutoUpdateCheck();
const AutoUpdateStatus &getAutoUpdateStatus();
String getCurrentFirmwareBuildId();
String getCurrentSpiffsBuildId();