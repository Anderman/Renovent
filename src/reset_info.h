#pragma once

#include <Arduino.h>

struct ResetInfoStatus {
  uint32_t bootCount;
  uint32_t rawReason;
  const char *reason;
  const char *detail;
  bool crashLikely;
  bool coreDumpPresent;
  uint32_t coreDumpSize;
  const char *coreDumpState;
  const char *coreDumpReason;
  const char *coreDumpBacktrace;
  bool coreDumpBacktraceCorrupted;
};

void resetInfoSetup();
void resetInfoPrintToSerial();
ResetInfoStatus getResetInfoStatus();