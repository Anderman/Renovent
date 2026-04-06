#pragma once

#include <Arduino.h>

struct ResetInfoStatus {
  uint32_t bootCount;
  uint32_t rawReason;
  const char *reason;
  const char *detail;
  bool crashLikely;
};

void resetInfoSetup();
void resetInfoPrintToSerial();
ResetInfoStatus getResetInfoStatus();