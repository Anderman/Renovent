#include "menu/settings_menu.h"

#include <cstdio>
#include <cstring>

#include "core/app_config.h"
#include "display/display_text_utils.h"
#include "display/display_reader.h"
#include "menu/sensors_menu.h"
#include "input/key_writer.h"
#include "menu/setting_writer.h"

namespace
{
    enum class SettingsAction : uint8_t
    {
        AdvanceOnly,
        ReadCurrentEntry,
        CaptureCurrentValue,
        ReturnToEntry,
        SelectNextEntry,
        FinishRead,
    };

    enum class SettingStepResult : uint8_t
    {
        Pending,
        Advance,
        JumpToOpenEntry,
        JumpToExitMenu,
        Finish,
        Abort,
    };

    struct SettingsState
    {
        bool running = false;
        uint8_t count = 0;
        uint8_t currentStepIndex = 0;
        uint32_t phaseStartedMs = 0;
        bool stepStarted = false;
        bool keysReleased = false;
        char lastDisplayText[9] = {0};
        char firstEntryKey[4] = {0};
        char currentEntryKey[4] = {0};
        SettingValue values[64] = {};
    };

    struct SettingStep
    {
        const char *phaseName = "idle";
        KeyMask keyPressed = kKeyNone;
        uint32_t keyDownMs = 0;
        uint32_t settleMs = 0;
        SettingsAction action = SettingsAction::ReadCurrentEntry;
    };

    constexpr uint8_t kMaxSettingsMenuCount = 64;
    constexpr uint32_t kStepDisplayTimeoutMs = 2000;
    constexpr uint8_t kOpenEntryStepIndex = 2;
    constexpr uint8_t kExitMenuStepIndex = 5;

    constexpr SettingStep kReadScript[] = {
        {"enter-settings-menu", kKeyFunction, app_config::kMenuEnterHoldMs, 100, SettingsAction::AdvanceOnly},
        {"enter-sensors-settings", static_cast<KeyMask>(kKeyFunction | kKeyOk), app_config::kMenuEnterHoldMs, 100, SettingsAction::ReadCurrentEntry},
        {"open-entry", kKeyOk, 500, 100, SettingsAction::CaptureCurrentValue},
        {"return-entry", kKeyOk, 160, 220, SettingsAction::ReturnToEntry},
        {"next-entry", kKeyPlus, 120, 150, SettingsAction::SelectNextEntry},
        {"exit-menu", kKeyFunction, app_config::kMenuExitHoldMs, 100, SettingsAction::FinishRead},
    };

    constexpr uint8_t kReadScriptStepCount = sizeof(kReadScript) / sizeof(kReadScript[0]);

    SettingsState g_state;
    SettingValue g_lastCompletedValues[64] = {};
    uint8_t g_lastCompletedCount = 0;
    char g_lastCompletedDisplayText[9] = {0};
    uint32_t g_lastCompletedMs = 0;
    bool g_startupReadPending = false;

    const char *currentPhaseName()
    {
        if (!g_state.running)
        {
            return "idle";
        }

        return kReadScript[g_state.currentStepIndex].phaseName;
    }

    bool isValidSettingsStartDisplay(const char *displayText)
    {
        return startsWithDisplay(displayText, "0.") ||
               startsWithDisplay(displayText, "1.") ||
               startsWithDisplay(displayText, "2.") ||
               startsWithDisplay(displayText, "3.");
    }

    int8_t findEntryIndexByKey(const char *key)
    {
        for (uint8_t index = 0; index < g_state.count; ++index)
        {
            if (std::strncmp(g_state.values[index].key, key, sizeof(g_state.values[index].key)) == 0)
            {
                return static_cast<int8_t>(index);
            }
        }

        return -1;
    }

    int8_t ensureEntryIndex(const char *key)
    {
        const int8_t existingIndex = findEntryIndexByKey(key);
        if (existingIndex >= 0)
        {
            return existingIndex;
        }

        if (g_state.count >= kMaxSettingsMenuCount)
        {
            return -1;
        }

        SettingValue &entry = g_state.values[g_state.count];
        entry = SettingValue{};
        std::strncpy(entry.key, key, sizeof(entry.key) - 1);
        entry.key[sizeof(entry.key) - 1] = '\0';
        ++g_state.count;
        return static_cast<int8_t>(g_state.count - 1U);
    }

    int8_t findCompletedEntryIndexByKey(const char *key)
    {
        for (uint8_t index = 0; index < g_lastCompletedCount; ++index)
        {
            if (std::strncmp(g_lastCompletedValues[index].key, key, sizeof(g_lastCompletedValues[index].key)) == 0)
            {
                return static_cast<int8_t>(index);
            }
        }

        return -1;
    }

    int8_t ensureCompletedEntryIndex(const char *key)
    {
        const int8_t existingIndex = findCompletedEntryIndexByKey(key);
        if (existingIndex >= 0)
        {
            return existingIndex;
        }

        if (g_lastCompletedCount >= kMaxSettingsMenuCount)
        {
            return -1;
        }

        SettingValue &entry = g_lastCompletedValues[g_lastCompletedCount];
        entry = SettingValue{};
        std::strncpy(entry.key, key, sizeof(entry.key) - 1);
        entry.key[sizeof(entry.key) - 1] = '\0';
        ++g_lastCompletedCount;
        return static_cast<int8_t>(g_lastCompletedCount - 1U);
    }

    bool setCurrentEntryKey(const char *displayText)
    {
        char parsedKey[4] = {0};
        if (!parseSettingKey(displayText, parsedKey))
        {
            return false;
        }

        const int8_t existingIndex = findEntryIndexByKey(parsedKey);
        if (existingIndex < 0 && ensureEntryIndex(parsedKey) < 0)
        {
            return false;
        }

        std::strncpy(g_state.currentEntryKey, parsedKey, sizeof(g_state.currentEntryKey) - 1);
        g_state.currentEntryKey[sizeof(g_state.currentEntryKey) - 1] = '\0';
        if (g_state.firstEntryKey[0] == '\0')
        {
            std::strncpy(g_state.firstEntryKey, parsedKey, sizeof(g_state.firstEntryKey) - 1);
            g_state.firstEntryKey[sizeof(g_state.firstEntryKey) - 1] = '\0';
        }
        return true;
    }

    bool captureCurrentValue(const char *displayText)
    {
        if (g_state.currentEntryKey[0] == '\0')
        {
            return false;
        }

        const int8_t entryIndex = ensureEntryIndex(g_state.currentEntryKey);
        if (entryIndex < 0)
        {
            return false;
        }

        SettingValue &entry = g_state.values[static_cast<uint8_t>(entryIndex)];
        ParsedSettingValue settingValue{};
        if (!getSettingValue(displayText, settingValue))
        {
            return false;
        }

        entry.available = true;
        std::strncpy(entry.key, g_state.currentEntryKey, sizeof(entry.key) - 1);
        entry.key[sizeof(entry.key) - 1] = '\0';
        copyDisplayText(entry.rawValue, settingValue.displayValue);
        entry.hasValue = settingValue.hasNumericValue;
        entry.value = settingValue.hasNumericValue ? settingValue.numericValue : 0;
        return true;
    }

    void pressKeys(uint32_t now, KeyMask keyMask)
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

    void advanceToStep(uint8_t stepIndex)
    {
        g_state.currentStepIndex = stepIndex;
        g_state.stepStarted = false;
        g_state.keysReleased = false;
    }

    void advanceToNextStep()
    {
        advanceToStep(static_cast<uint8_t>(g_state.currentStepIndex + 1U));
    }

    uint8_t currentValueCount()
    {
        return g_state.running ? g_state.count : g_lastCompletedCount;
    }

    DisplaySnapshot readCurrentDisplaySnapshot()
    {
        const DisplaySnapshot snapshot = getDisplaySnapshot();
        copyDisplayText(g_state.lastDisplayText, snapshot.text);
        return snapshot;
    }

    void finishRead()
    {
        pressKeys(kKeyNone);

        std::memcpy(g_lastCompletedValues, g_state.values, sizeof(g_lastCompletedValues));
        g_lastCompletedCount = g_state.count;
        copyDisplayText(g_lastCompletedDisplayText, g_state.lastDisplayText);
        g_lastCompletedMs = millis();

        g_startupReadPending = false;
        g_state = SettingsState{};
    }

    void abortRead()
    {
        pressKeys(kKeyNone);
        g_state = SettingsState{};
    }

    bool waitForPhase(uint32_t now, KeyMask keyMask, uint32_t keyDownMs, uint32_t settleMs)
    {
        if (!g_state.stepStarted)
        {
            pressKeys(now, keyMask);
            return false;
        }

        if (!g_state.keysReleased)
        {
            if (isWaitCompleted(now, keyDownMs))
            {
                releaseKeys(now);
            }
            return false;
        }

        if (!isWaitCompleted(now, settleMs))
        {
            return false;
        }

        return true;
    }

    SettingStepResult handleCurrentStepAction(const DisplaySnapshot &snapshot)
    {
        switch (kReadScript[g_state.currentStepIndex].action)
        {
        case SettingsAction::AdvanceOnly:
            return SettingStepResult::Advance;

        case SettingsAction::ReadCurrentEntry:
            return setCurrentEntryKey(snapshot.text) ? SettingStepResult::Advance : SettingStepResult::Pending;

        case SettingsAction::CaptureCurrentValue:
            return captureCurrentValue(snapshot.text) ? SettingStepResult::Advance : SettingStepResult::Pending;

        case SettingsAction::ReturnToEntry:
            return SettingStepResult::Advance;

        case SettingsAction::SelectNextEntry:
        {
            char parsedKey[4] = {0};
            if (!parseSettingKey(snapshot.text, parsedKey))
            {
                return SettingStepResult::Pending;
            }

            if (std::strncmp(parsedKey, g_state.currentEntryKey, sizeof(parsedKey)) == 0)
            {
                return SettingStepResult::Pending;
            }

            if (g_state.firstEntryKey[0] != '\0' &&
                std::strncmp(parsedKey, g_state.firstEntryKey, sizeof(parsedKey)) == 0)
            {
                return SettingStepResult::JumpToExitMenu;
            }

            if (!setCurrentEntryKey(snapshot.text))
            {
                return SettingStepResult::Abort;
            }

            return SettingStepResult::JumpToOpenEntry;
        }

        case SettingsAction::FinishRead:
            return SettingStepResult::Finish;
        }

        return SettingStepResult::Abort;
    }

    void runCurrentStep(uint32_t now)
    {
        const SettingStep &step = kReadScript[g_state.currentStepIndex];
        if (!waitForPhase(now, step.keyPressed, step.keyDownMs, step.settleMs))
        {
            return;
        }

        const DisplaySnapshot snapshot = readCurrentDisplaySnapshot();

        switch (handleCurrentStepAction(snapshot))
        {
        case SettingStepResult::Pending:
            if (isWaitCompleted(now, kStepDisplayTimeoutMs))
            {
                abortRead();
            }
            return;

        case SettingStepResult::Advance:
            advanceToNextStep();
            return;

        case SettingStepResult::JumpToOpenEntry:
            advanceToStep(kOpenEntryStepIndex);
            return;

        case SettingStepResult::JumpToExitMenu:
            advanceToStep(kExitMenuStepIndex);
            return;

        case SettingStepResult::Finish:
            finishRead();
            return;

        case SettingStepResult::Abort:
            abortRead();
            return;
        }
    }
} // namespace

void settingsMenuSetup()
{
    g_state = SettingsState{};
    g_startupReadPending = true;
}

void settingsMenuLoop()
{
    if (!g_state.running)
    {
        if (g_startupReadPending)
        {
            requestSettingsMenuRead();
        }
        return;
    }
    runCurrentStep(millis());
}

bool requestSettingsMenuRead()
{
    if (g_state.running || sensorsMenuIsBusy() || settingWriterIsBusy())
    {
        return false;
    }

    const DisplaySnapshot snapshot = getDisplaySnapshot();
    if (!isValidSettingsStartDisplay(snapshot.text))
    {
        return false;
    }
    g_state.running = true;
    g_state.currentStepIndex = 0;
    return true;
}

bool settingsMenuIsBusy()
{
    return g_state.running;
}

void updateSettingsMenuValueFromWrite(const char *key, const char *rawValue)
{
    if (key == nullptr || key[0] == '\0' || rawValue == nullptr)
    {
        return;
    }

    const int8_t entryIndex = ensureCompletedEntryIndex(key);
    if (entryIndex < 0)
    {
        return;
    }

    SettingValue &entry = g_lastCompletedValues[static_cast<uint8_t>(entryIndex)];
    ParsedSettingValue settingValue{};
    if (!getSettingValue(rawValue, settingValue))
    {
        return;
    }

    entry.available = true;
    std::strncpy(entry.key, key, sizeof(entry.key) - 1);
    entry.key[sizeof(entry.key) - 1] = '\0';
    copyDisplayText(entry.rawValue, settingValue.displayValue);
    entry.hasValue = settingValue.hasNumericValue;
    entry.value = settingValue.hasNumericValue ? settingValue.numericValue : 0;
    g_lastCompletedMs = millis();
}

SettingsMenuHaStatus getSettingsMenuHaStatus()
{
    SettingsMenuHaStatus status{};

    status.count = currentValueCount();
    status.lastCompletedMs = g_lastCompletedMs;
    std::memcpy(status.values, g_lastCompletedValues, sizeof(status.values));

    if (g_state.running)
    {
        for (uint8_t index = 0; index < g_state.count; ++index)
        {
            if (g_state.values[index].available)
            {
                status.values[index] = g_state.values[index];
            }
        }
    }

    return status;
}

SettingsMenuStatus getSettingsMenuWebStatus()
{
    SettingsMenuStatus status{};

    status.running = g_state.running;
    status.count = currentValueCount();
    status.lastCompletedMs = g_lastCompletedMs;
    std::strncpy(status.phase, currentPhaseName(), sizeof(status.phase) - 1);
    status.phase[sizeof(status.phase) - 1] = '\0';
    copyDisplayText(status.lastDisplayText, g_state.running ? g_state.lastDisplayText : g_lastCompletedDisplayText);

    return status;
}

bool getSettingsMenuWebValue(uint8_t index, SettingsMenuValue &value)
{
    if (index >= kMaxSettingsMenuCount)
    {
        return false;
    }

    value = g_lastCompletedValues[index];
    if (g_state.running && g_state.values[index].available)
    {
        value = g_state.values[index];
    }

    return value.available;
}