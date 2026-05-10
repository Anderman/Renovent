#include "menu/setting_writer.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "display/display_reader.h"
#include "display/display_text_utils.h"
#include "input/key_writer.h"
#include "menu/parameter_definitions.h"
#include "menu/sensors_menu.h"
#include "menu/settings_menu.h"
#include "menu/setting_writer_internal.h"
#include "menu/setting_writer_steps.h"
#include "webui/text_log.h"

namespace setting_writer_internal
{

  SettingWriterState g_state;
  char g_lastCompletedKey[4] = {0};
  uint32_t g_lastCompletedMs = 0;
  int32_t g_lastCompletedValue = 0;

  bool isKnownSettingKey(const char *key)
  {
    for (size_t index = 0; index < getParameterDefinitionCount(); ++index)
    {
      const ParameterDefinition *definition = getParameterDefinitionAt(index);
      if (definition != nullptr && std::strcmp(definition->key, key) == 0)
      {
        return true;
      }
    }

    return false;
  }

  bool tryParseKey(const char *rawKey, char (&parsedKey)[4])
  {
    if (rawKey == nullptr || !isKnownSettingKey(rawKey))
    {
      return false;
    }

    std::snprintf(parsedKey, sizeof(parsedKey), "%s", rawKey);
    return true;
  }

  SettingWriteStatus scheduleWriteRequest(const char *haKey, const char *displayValue, bool hasNumericValue, int32_t numericValue)
  {
    if (g_state.running || sensorsMenuIsBusy() || settingsMenuIsBusy())
    {
      return SettingWriteStatus::Busy;
    }

    const DisplaySnapshot snapshot = getDisplaySnapshot();
    if (!isStartDisplay(snapshot.text))
    {
      return SettingWriteStatus::InvalidStartDisplay;
    }

    SettingWriteRequest request{};
    if (!tryParseKey(haKey, request.key))
    {
      return SettingWriteStatus::InvalidKey;
    }

    ParsedSettingValue settingValue{};
    if (!tryGetInputSettingValue(displayValue, settingValue))
    {
      return SettingWriteStatus::InvalidKey;
    }

    request.targetHasNumericValue = hasNumericValue && settingValue.hasNumericValue;
    request.targetValue = request.targetHasNumericValue ? numericValue : 0;
    copyDisplayText(request.targetDisplayValue, displayValue);

    g_state = SettingWriterState{};
    g_state.running = true;
    g_state.request = request;
    return SettingWriteStatus::Scheduled;
  }

  void startStep(uint32_t now, uint32_t keyMask)
  {
    g_state.phaseStartedMs = now;
    g_state.stepStarted = true;
    g_state.keysReleased = false;
    pressKeys(static_cast<KeyMask>(keyMask));
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

    std::memcpy(g_lastCompletedKey, g_state.request.key, sizeof(g_lastCompletedKey));
    g_lastCompletedMs = millis();
    g_lastCompletedValue = g_state.currentValue;
    textLogAddf("HA publish pending key=%s display=%s value=%ld", g_state.request.key, g_state.request.targetDisplayValue, static_cast<long>(g_lastCompletedValue));
    updateSettingsMenuValueFromWrite(g_state.request.key, g_state.request.targetDisplayValue);

    g_state = SettingWriterState{};
  }

  void abortWrite()
  {
    pressKeys(kKeyNone);
    g_state = SettingWriterState{};
  }

  bool updateInvalidDisplayTimer(uint32_t now)
  {
    if (g_state.invalidDisplayStartedMs == 0)
    {
      g_state.invalidDisplayStartedMs = now;
    }

    if (static_cast<uint32_t>(now - g_state.invalidDisplayStartedMs) >= 2000)
    {
      abortWrite();
      return true;
    }

    return false;
  }

} // namespace setting_writer_internal

void settingWriterSetup()
{
  setting_writer_internal::g_state = setting_writer_internal::SettingWriterState{};
}

void settingWriterLoop()
{
  if (!setting_writer_internal::g_state.running)
  {
    return;
  }

  setting_writer_internal::runCurrentStep(millis());
}

SettingWriteStatus writeSetting(const char *haKey, int32_t value)
{
  using namespace setting_writer_internal;

  char displayValue[9] = {0};
  std::snprintf(displayValue, sizeof(displayValue), "%ld", static_cast<long>(value));
  return scheduleWriteRequest(haKey, displayValue, true, value);
}

SettingWriteStatus writeSetting(const char *haKey, const char *displayValue)
{
  using namespace setting_writer_internal;

  if (displayValue == nullptr || displayValue[0] == '\0')
  {
    return SettingWriteStatus::InvalidKey;
  }

  ParsedSettingValue settingValue{};
  if (!tryGetInputSettingValue(displayValue, settingValue))
  {
    return SettingWriteStatus::InvalidKey;
  }

  const int32_t numericValue = settingValue.numericValue;
  const bool hasNumericValue = settingValue.hasNumericValue;
  return scheduleWriteRequest(haKey, displayValue, hasNumericValue, numericValue);
}

bool settingWriterIsBusy()
{
  return setting_writer_internal::g_state.running;
}

SettingWriterStatus getSettingWriterStatus()
{
  using namespace setting_writer_internal;

  SettingWriterStatus status{};

  status.running = g_state.running;
  std::memcpy(status.key, g_state.running ? g_state.request.key : g_lastCompletedKey, sizeof(status.key));
  status.value = g_state.running ? g_state.currentValue : g_lastCompletedValue;
  status.lastCompletedMs = g_lastCompletedMs;

  return status;
}