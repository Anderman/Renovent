#include "setting_writer.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "app_config.h"
#include "display_reader.h"
#include "display_text_utils.h"
#include "sensors_menu.h"
#include "key_writer.h"
#include "settings_menu.h"

namespace
{
  enum class SettingWriterStepKind : uint8_t
  {
    FixedKey,
    NavigateToEntry,
    AdjustValue,
  };

  struct SettingWriteRequest
  {
    char key[4] = {0};
    int32_t targetValue = 0;
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

  struct SettingWriterState
  {
    bool running = false;
    uint8_t currentStepIndex = 0;
    uint32_t phaseStartedMs = 0;
    uint32_t invalidDisplayStartedMs = 0;
    bool stepStarted = false;
    bool keysReleased = false;
    int32_t currentValue = 0;
    char previousDisplayText[9] = {0};
    char lastDisplayText[9] = {0};
    SettingWriteRequest request = {};
  };

  constexpr uint32_t kStepDisplayTimeoutMs = 2000;
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

  portMUX_TYPE g_settingWriterMux = portMUX_INITIALIZER_UNLOCKED;
  SettingWriterState g_state;
  char g_lastCompletedKey[4] = {0};
  char g_lastCompletedDisplayText[9] = {0};
  uint32_t g_lastCompletedMs = 0;
  int32_t g_lastCompletedValue = 0;
  int32_t g_lastTargetValue = 0;

  bool isValidSettingsStartDisplay(const char *displayText)
  {
    return startsWithDisplay(displayText, "0.") ||
           startsWithDisplay(displayText, "1.") ||
           startsWithDisplay(displayText, "2.") ||
           startsWithDisplay(displayText, "3.");
  }

  const SettingWriterStep *currentScript(uint8_t &stepCount)
  {
    stepCount = sizeof(kWriteScript) / sizeof(kWriteScript[0]);
    return kWriteScript;
  }

  const char *currentPhaseName()
  {
    if (!g_state.running)
    {
      return "idle";
    }

    uint8_t stepCount = 0;
    const SettingWriterStep *script = currentScript(stepCount);
    return script[g_state.currentStepIndex].phaseName;
  }

  bool parseRequestedKey(const char *rawKey, SettingWriteRequest &request)
  {
    if (rawKey == nullptr)
    {
      return false;
    }

    const size_t length = std::strlen(rawKey);
    if (length < 2 || length > 3)
    {
      return false;
    }

    const char prefix = static_cast<char>(std::toupper(static_cast<unsigned char>(rawKey[0])));
    if (prefix != 'U' && prefix != 'I' && prefix != 'P')
    {
      return false;
    }

    int number = 0;
    for (size_t index = 1; index < length; ++index)
    {
      const char ch = rawKey[index];
      if (ch < '0' || ch > '9')
      {
        return false;
      }
      number = number * 10 + (ch - '0');
    }

    if (prefix == 'U')
    {
      if (number < 1 || number > 8)
      {
        return false;
      }
    }
    else if (prefix == 'I')
    {
      if (number < 1 || number > 19)
      {
        return false;
      }
    }
    else
    {
      if (number < 1 || number > 17)
      {
        return false;
      }
    }

    std::snprintf(request.key, sizeof(request.key), "%c%d", prefix, number);
    return true;
  }

  void startStep(uint32_t now, KeyMask keyMask)
  {
    g_state.phaseStartedMs = now;
    g_state.stepStarted = true;
    g_state.keysReleased = false;
    pressKeys(keyMask);
  }

  bool isWaitCompleted(uint32_t now, uint32_t ms)
  {
    return static_cast<uint32_t>(now - g_state.phaseStartedMs) >= ms;
  }

  void releaseKeys(uint32_t now)
  {
    pressKeys(kKeyNone);
    g_state.keysReleased = true;
    g_state.phaseStartedMs = now;
  }

  void advanceToNextStep()
  {
    ++g_state.currentStepIndex;
    g_state.stepStarted = false;
    g_state.keysReleased = false;
    g_state.invalidDisplayStartedMs = 0;
  }

  void finishWrite()
  {
    pressKeys(kKeyNone);

    portENTER_CRITICAL(&g_settingWriterMux);
    std::memcpy(g_lastCompletedKey, g_state.request.key, sizeof(g_lastCompletedKey));
    copyDisplayText(g_lastCompletedDisplayText, g_state.lastDisplayText);
    g_lastCompletedMs = millis();
    g_lastCompletedValue = g_state.currentValue;
    g_lastTargetValue = g_state.request.targetValue;
    portEXIT_CRITICAL(&g_settingWriterMux);

    g_state = SettingWriterState{};
  }

  void abortWrite()
  {
    pressKeys(kKeyNone);
    g_state = SettingWriterState{};
  }

  bool parseCurrentDisplayValue(const char *displayText, int32_t &value)
  {
    return parseLastNumber(displayText, value);
  }

  bool updateInvalidDisplayTimer(uint32_t now)
  {
    if (g_state.invalidDisplayStartedMs == 0)
    {
      g_state.invalidDisplayStartedMs = now;
    }

    if (static_cast<uint32_t>(now - g_state.invalidDisplayStartedMs) >= kStepDisplayTimeoutMs)
    {
      abortWrite();
      return true;
    }

    return false;
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
      if (!parseCurrentDisplayValue(snapshot.text, currentValue))
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
      if (parseCurrentDisplayValue(snapshot.text, currentValue))
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
} // namespace

void settingWriterSetup()
{
  g_state = SettingWriterState{};
}

void settingWriterLoop()
{
  if (!g_state.running)
  {
    return;
  }

  runCurrentStep(millis());
}

SettingWriteResult requestSettingWriteDetailed(const char *key, int32_t value)
{
  SettingWriteResult result{};
  result.scheduled = false;
  result.rejectReason = SettingWriteRejectReason::None;
  result.targetValue = value;

  if (g_state.running || sensorsMenuIsBusy() || settingsMenuIsBusy())
  {
    result.rejectReason = SettingWriteRejectReason::Busy;
    copyDisplayText(result.displayText, getDisplaySnapshot().text);
    return result;
  }

  SettingWriteRequest request{};
  if (!parseRequestedKey(key, request))
  {
    result.rejectReason = SettingWriteRejectReason::InvalidKey;
    copyDisplayText(result.displayText, getDisplaySnapshot().text);
    return result;
  }

  std::memcpy(result.key, request.key, sizeof(result.key));

  const DisplaySnapshot snapshot = getDisplaySnapshot();
  copyDisplayText(result.displayText, snapshot.text);
  if (!isValidSettingsStartDisplay(snapshot.text))
  {
    result.rejectReason = SettingWriteRejectReason::InvalidStartDisplay;
    return result;
  }

  request.targetValue = value;
  g_state = SettingWriterState{};
  g_state.running = true;
  g_state.request = request;
  copyDisplayText(g_state.lastDisplayText, snapshot.text);
  result.scheduled = true;
  result.rejectReason = SettingWriteRejectReason::None;
  return result;
}

bool requestSettingWrite(const char *key, int32_t value)
{
  return requestSettingWriteDetailed(key, value).scheduled;
}

bool settingWriterIsBusy()
{
  return g_state.running;
}

SettingWriterStatus getSettingWriterStatus()
{
  SettingWriterStatus status{};

  portENTER_CRITICAL(&g_settingWriterMux);
  status.running = g_state.running;
  std::memcpy(status.key, g_state.running ? g_state.request.key : g_lastCompletedKey, sizeof(status.key));
  status.currentValue = g_state.running ? g_state.currentValue : g_lastCompletedValue;
  status.targetValue = g_state.running ? g_state.request.targetValue : g_lastTargetValue;
  status.lastCompletedMs = g_lastCompletedMs;
  std::strncpy(status.phase, currentPhaseName(), sizeof(status.phase) - 1);
  status.phase[sizeof(status.phase) - 1] = '\0';
  copyDisplayText(status.lastDisplayText,
                  g_state.running ? g_state.lastDisplayText : g_lastCompletedDisplayText);
  portEXIT_CRITICAL(&g_settingWriterMux);

  return status;
}