#include "setting_writer_internal.h"
#include "setting_writer_steps.h"

#include <cstring>

#include "app_config.h"
#include "display_reader.h"
#include "display_text_utils.h"

namespace setting_writer_internal {
namespace {

constexpr uint32_t kNavigationKeyDownMs = 160;
constexpr uint32_t kNavigationSettleMs = 500;
constexpr uint32_t kAdjustHoldTimeoutMs = 4000;

constexpr SettingWriterStep kWriteScript[] = {
    {"enter-settings-menu", SettingWriterStepKind::FixedKey, kKeyFunction, app_config::kMenuEnterHoldMs, 100, "U0"},
    {"enter-installer-menu", SettingWriterStepKind::FixedKey, static_cast<KeyMask>(kKeyFunction | kKeyOk), app_config::kMenuEnterHoldMs, 500, nullptr},
    {"navigate-to-entry", SettingWriterStepKind::NavigateToEntry, kKeyPlus, 0, 0, nullptr},
    {"open-entry", SettingWriterStepKind::FixedKey, kKeyOk, 450, 100, nullptr},
    {"adjust-value", SettingWriterStepKind::AdjustValue, kKeyNone, 0, 150, nullptr},
    {"save-value", SettingWriterStepKind::FixedKey, static_cast<KeyMask>(kKeyFunction | kKeyPlus), 450, 200, nullptr},
    {"confirm-value", SettingWriterStepKind::FixedKey, kKeyOk, 160, 100, nullptr},
    {"exit-menu", SettingWriterStepKind::FixedKey, kKeyFunction, app_config::kMenuExitHoldMs, 100, nullptr},
};

const SettingWriterStep *currentScript(uint8_t &stepCount)
{
  stepCount = sizeof(kWriteScript) / sizeof(kWriteScript[0]);
  return kWriteScript;
}

void runFixedStep(uint32_t now, const SettingWriterStep &step)
{
  if (!g_state.stepStarted)
  {
    startStep(now, step.keyPressed);
    return;
  }

  if (!g_state.keysReleased)
  {
    if (isWaitCompleted(now, step.keyDownMs))
    {
      releaseKeys(now);
    }
    return;
  }

  if (!isWaitCompleted(now, step.settleMs))
  {
    return;
  }

  const DisplaySnapshot snapshot = getDisplaySnapshot();
  copyDisplayText(g_state.lastDisplayText, snapshot.text);
  if (!startsWithDisplay(snapshot.text, step.expectedDisplayPrefix))
  {
    if (step.expectedDisplayPrefix != nullptr && updateInvalidDisplayTimer(now))
    {
      return;
    }

    if (step.expectedDisplayPrefix != nullptr)
    {
      return;
    }
  }

  g_state.invalidDisplayStartedMs = 0;
  advanceToNextStep();
}

void runNavigateToEntryStep(uint32_t now)
{
  if (!g_state.stepStarted)
  {
    const DisplaySnapshot snapshot = getDisplaySnapshot();
    copyDisplayText(g_state.previousDisplayText, snapshot.text);
    copyDisplayText(g_state.lastDisplayText, snapshot.text);

    char displayedKey[4] = {0};
    if (parseDisplayKey(snapshot.text, displayedKey) &&
        std::strncmp(displayedKey, g_state.request.key, sizeof(displayedKey)) == 0)
    {
      g_state.invalidDisplayStartedMs = 0;
      advanceToNextStep();
      return;
    }

    startStep(now, kKeyPlus);
    return;
  }

  if (!g_state.keysReleased)
  {
    if (isWaitCompleted(now, kNavigationKeyDownMs))
    {
      releaseKeys(now);
    }
    return;
  }

  if (!isWaitCompleted(now, kNavigationSettleMs))
  {
    return;
  }

  const DisplaySnapshot snapshot = getDisplaySnapshot();
  copyDisplayText(g_state.lastDisplayText, snapshot.text);
  if (std::strncmp(snapshot.text, g_state.previousDisplayText, sizeof(g_state.previousDisplayText)) == 0)
  {
    updateInvalidDisplayTimer(now);
    return;
  }

  char displayedKey[4] = {0};
  if (!parseDisplayKey(snapshot.text, displayedKey))
  {
    updateInvalidDisplayTimer(now);
    return;
  }

  g_state.invalidDisplayStartedMs = 0;

  if (std::strncmp(displayedKey, g_state.request.key, sizeof(displayedKey)) == 0)
  {
    advanceToNextStep();
    return;
  }

  copyDisplayText(g_state.previousDisplayText, snapshot.text);
  g_state.stepStarted = false;
  g_state.keysReleased = false;
}

void runAdjustValueStep(uint32_t now, const SettingWriterStep &step)
{
  if (!g_state.stepStarted)
  {
    const DisplaySnapshot snapshot = getDisplaySnapshot();
    copyDisplayText(g_state.lastDisplayText, snapshot.text);

    int32_t currentValue = 0;
    if (!parseLastNumber(snapshot.text, currentValue))
    {
      updateInvalidDisplayTimer(now);
      return;
    }

    g_state.invalidDisplayStartedMs = 0;
    g_state.currentValue = currentValue;
    if (g_state.currentValue == g_state.request.targetValue)
    {
      advanceToNextStep();
      return;
    }

    startStep(now, g_state.currentValue < g_state.request.targetValue ? kKeyPlus : kKeyMinus);
    return;
  }

  if (!g_state.keysReleased)
  {
    const DisplaySnapshot snapshot = getDisplaySnapshot();
    copyDisplayText(g_state.lastDisplayText, snapshot.text);

    int32_t currentValue = 0;
    if (parseLastNumber(snapshot.text, currentValue))
    {
      g_state.invalidDisplayStartedMs = 0;
      g_state.currentValue = currentValue;
    }

    const bool increasing = g_state.currentValue < g_state.request.targetValue;
    const bool reachedTarget = increasing
                                   ? g_state.currentValue >= g_state.request.targetValue
                                   : g_state.currentValue <= g_state.request.targetValue;
    if (reachedTarget || isWaitCompleted(now, kAdjustHoldTimeoutMs))
    {
      releaseKeys(now);
    }
    return;
  }

  if (!isWaitCompleted(now, step.settleMs))
  {
    return;
  }

  g_state.stepStarted = false;
  g_state.keysReleased = false;
}

} // namespace

void runCurrentStep(uint32_t now)
{
  uint8_t stepCount = 0;
  const SettingWriterStep *script = currentScript(stepCount);
  const SettingWriterStep &step = script[g_state.currentStepIndex];

  switch (step.kind)
  {
  case SettingWriterStepKind::FixedKey:
    runFixedStep(now, step);
    break;
  case SettingWriterStepKind::NavigateToEntry:
    runNavigateToEntryStep(now);
    break;
  case SettingWriterStepKind::AdjustValue:
    runAdjustValueStep(now, step);
    break;
  }

  if (!g_state.running)
  {
    return;
  }

  if (g_state.currentStepIndex == stepCount)
  {
    finishWrite();
  }
}

} // namespace setting_writer_internal
