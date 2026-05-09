#pragma once

#include <Arduino.h>

#include "key_writer.h"

namespace setting_writer_internal {

enum class SettingWriterStepKind : uint8_t
{
  FixedKey,
  NavigateToEntry,
  AdjustValue,
};

struct SettingWriterStep
{
  const char *phaseName = "idle";
  SettingWriterStepKind kind = SettingWriterStepKind::FixedKey;
  KeyMask keyPressed = kKeyNone;
  uint32_t keyDownMs = 0;
  uint32_t settleMs = 0;
  const char *expectedDisplayPrefix = nullptr;
};

void runCurrentStep(uint32_t now);

} // namespace setting_writer_internal