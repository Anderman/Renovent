#include "sensors_menu.h"

#include <cstring>

#include "app_config.h"
#include "display_text_utils.h"
#include "display_reader.h"
#include "key_writer.h"
#include "setting_writer.h"
#include "settings_menu.h"

namespace
{
    constexpr uint8_t kMaxUnknownSensors = 8;

    enum class SensorsAction : uint8_t
    {
        ReadCurrentEntry,
        SelectNextEntry,
        FinishRead,
    };

    enum class SensorsStepResult : uint8_t
    {
        Pending,
        JumpToNextEntry,
        JumpToExitMenu,
        Finish,
        Abort,
    };

    struct SensorsMenuState
    {
        bool running = false;
        bool done = false;
        uint8_t currentStep = 0;
        uint8_t currentScriptIndex = 0;
        uint32_t phaseStartedMs = 0;
        bool stepStarted = false;
        bool keysReleased = false;
        char lastDisplayText[9] = {0};
        char firstEntryKey[4] = {0};
        char currentEntryKey[4] = {0};
        SensorsMenuCapturedEntry entries[13] = {};
        SensorsMenuUnknownEntry unknownEntries[kMaxUnknownSensors] = {};
    };

    struct SensorsMenuStep
    {
        const char *phaseName = "idle";
        KeyMask keyPressed = kKeyNone;
        uint32_t keyDownMs = 0;
        uint32_t settleMs = 0;
        SensorsAction action = SensorsAction::ReadCurrentEntry;
    };

    constexpr uint8_t kFirstSensorsStep = 1;
    constexpr uint8_t kLastSensorsStep = 13;
    constexpr uint8_t kSensorsStepCount = kLastSensorsStep - kFirstSensorsStep + 1U;
    constexpr uint8_t kLogicalValueCount = 14;
    constexpr uint32_t kAutoScanIntervalMs = 60000;
    constexpr uint32_t kStepDisplayTimeoutMs = 2000;
    constexpr uint8_t kNextEntryStepIndex = 1;
    constexpr uint8_t kExitMenuStepIndex = 2;

    constexpr SensorsMenuDefinition kMenuDefinitions[13] = {
        {"2.200", "Actuele stand/afvoervolume", "Standenschakelaar 1, 2 of 3 plus ingesteld afvoervolume [m3/h]."},
        {"C 0", "Meldcode bedrijfssituatie", "C0 geen melding, C3/C6 constant druk, C7 correctie maximale luchtvolume."},
        {"bP.1", "Status bypass", "Alleen indien gemonteerd: 0 dicht, 1 automatisch, 2 toevoer minimaal."},
        {"tP.9", "Temperatuur van buiten", "Bij negatieve temperatuur kan de uitlezing afwijken van het voorbeeld."},
        {"ts.21", "Temperatuur van binnen", "Temperatuurweergave in graden Celsius."},
        {"In.0", "n.v.t.", "Volgens handleiding niet van toepassing."},
        {"u.156", "Actueel toevoervolume", "Actueel toevoervolume [m3/h]."},
        {"u.156", "Actueel afvoervolume", "Actueel afvoervolume [m3/h]."},
        {"t.180", "Actuele druk toevoerkanaal", "Actuele druk [Pa]."},
        {"A.180", "Actuele druk afvoerkanaal", "Actuele druk [Pa]."},
        {"u0.0", "Status vorstbeveiliging", "0 niet, 1 t/m 4 onbalans, 5 toevoerventilator uit."},
        {"st.9", "Temperatuur naar buiten", "Wanneer voeler niet is aangesloten toont de unit een vaste fallbackwaarde."},
        {"Pt.18", "Temperatuur naar binnen", "Wanneer voeler niet is aangesloten toont de unit een vaste fallbackwaarde."},
    };

    constexpr SensorsMenuValueDefinition kValueDefinitions[kLogicalValueCount] = {
        {"fan_mode", "Actuele stand", "", "Standenschakelaar 1, 2 of 3."},
        {"exhaust_setpoint", "Ingesteld afvoervolume", "m3/h", "Ingesteld afvoervolume [m3/h]."},
        {"operation_code", "Meldcode bedrijfssituatie", "", "C0 geen melding, C3/C6 constant druk, C7 correctie maximale luchtvolume."},
        {"bypass_status", "Status bypass", "", "0 dicht, 1 automatisch, 2 toevoer minimaal."},
        {"outside_temperature", "Temperatuur van buiten", "C", "Temperatuur van buiten in graden Celsius."},
        {"inside_temperature", "Temperatuur van binnen", "C", "Temperatuur van binnen in graden Celsius."},
        {"input_status", "n.v.t.", "", "Volgens handleiding niet van toepassing."},
        {"supply_flow", "Actueel toevoervolume", "m3/h", "Actueel toevoervolume [m3/h]."},
        {"exhaust_flow", "Actueel afvoervolume", "m3/h", "Actueel afvoervolume [m3/h]."},
        {"supply_pressure", "Actuele druk toevoerkanaal", "Pa", "Actuele druk [Pa]."},
        {"exhaust_pressure", "Actuele druk afvoerkanaal", "Pa", "Actuele druk [Pa]."},
        {"frost_protection", "Status vorstbeveiliging", "", "0 niet, 1 t/m 4 onbalans, 5 toevoerventilator uit."},
        {"outgoing_temperature", "Temperatuur naar buiten", "C", "Fallbackwaarde als voeler niet is aangesloten."},
        {"incoming_temperature", "Temperatuur naar binnen", "C", "Fallbackwaarde als voeler niet is aangesloten."},
    };

    constexpr SensorsMenuStep kReadScript[] = {
        {"enter-sensors-menu", static_cast<KeyMask>(kKeyFunction | kKeyOk), app_config::kMenuEnterHoldMs, 100, SensorsAction::ReadCurrentEntry},
        {"next-entry", kKeyPlus, 150, 100, SensorsAction::SelectNextEntry},
        {"exit-menu", kKeyFunction, 1000, 100, SensorsAction::FinishRead},
    };

    constexpr uint8_t kReadScriptStepCount = sizeof(kReadScript) / sizeof(kReadScript[0]);

    portMUX_TYPE g_menuMux = portMUX_INITIALIZER_UNLOCKED;
    SensorsMenuState g_scanState;
    SensorsMenuCapturedEntry g_lastCompletedEntries[13] = {};
    SensorsMenuUnknownEntry g_lastCompletedUnknownEntries[kMaxUnknownSensors] = {};
    char g_lastCompletedDisplayText[9] = {0};
    uint32_t g_lastCompletedMs = 0;
    uint32_t g_lastScanStartedMs = 0;
    bool g_startupScanPending = false;

    bool autoScanAllowed()
    {
        return app_config::kSensorsMenuAutoScanEnabled;
    }

    const char *currentPhaseName()
    {
        if (g_scanState.running)
        {
            return kReadScript[g_scanState.currentScriptIndex].phaseName;
        }

        if (g_scanState.done)
        {
            return "done";
        }

        return "idle";
    }

    void clearDetail(char (&detail)[32])
    {
        detail[0] = '\0';
    }

    bool parseSensorEntryKey(const char *displayText, char (&key)[4])
    {
        if (displayText == nullptr)
        {
            return false;
        }

        uint8_t readIndex = 0;
        while (displayText[readIndex] == ' ')
        {
            ++readIndex;
        }

        uint8_t writeIndex = 0;
        while (displayText[readIndex] != '\0' && writeIndex < sizeof(key) - 1U)
        {
            const char current = displayText[readIndex++];
            if (current == ' ')
            {
                if (writeIndex == 0)
                {
                    continue;
                }
                break;
            }

            if (current == '.')
            {
                break;
            }

            if ((current >= 'a' && current <= 'z'))
            {
                key[writeIndex++] = static_cast<char>(current - 'a' + 'A');
                continue;
            }

            if ((current >= 'A' && current <= 'Z') || (current >= '0' && current <= '9'))
            {
                key[writeIndex++] = current;
                continue;
            }

            break;
        }

        key[writeIndex] = '\0';
        return writeIndex > 0U;
    }

    uint8_t sensorStepForKey(const char *key)
    {
        if (key == nullptr || key[0] == '\0')
        {
            return 0;
        }

        if ((key[0] >= '1' && key[0] <= '3') && key[1] == '\0')
        {
            return 1;
        }

        if (std::strcmp(key, "C") == 0)
        {
            return 2;
        }
        if (std::strcmp(key, "BP") == 0)
        {
            return 3;
        }
        if (std::strcmp(key, "TP") == 0)
        {
            return 4;
        }
        if (std::strcmp(key, "TS") == 0)
        {
            return 5;
        }
        if (std::strcmp(key, "IN") == 0)
        {
            return 6;
        }
        if (std::strcmp(key, "N") == 0)
        {
            return 7;
        }
        if (std::strcmp(key, "U") == 0)
        {
            return 8;
        }
        if (std::strcmp(key, "T") == 0)
        {
            return 9;
        }
        if (std::strcmp(key, "A") == 0)
        {
            return 10;
        }
        if (std::strcmp(key, "U0") == 0)
        {
            return 11;
        }
        if (std::strcmp(key, "ST") == 0)
        {
            return 12;
        }
        if (std::strcmp(key, "PT") == 0)
        {
            return 13;
        }

        return 0;
    }

    bool parseFirstAndLastNumber(const char *rawValue, int32_t &firstValue, int32_t &lastValue)
    {
        int32_t currentValue = 0;
        bool inNumber = false;
        bool sawNumber = false;
        bool firstCaptured = false;

        for (uint8_t index = 0; rawValue[index] != '\0'; ++index)
        {
            const char current = rawValue[index];
            if (current >= '0' && current <= '9')
            {
                if (!inNumber)
                {
                    currentValue = 0;
                    inNumber = true;
                }
                currentValue = currentValue * 10 + (current - '0');
                sawNumber = true;
                continue;
            }

            if (inNumber)
            {
                if (!firstCaptured)
                {
                    firstValue = currentValue;
                    firstCaptured = true;
                }
                lastValue = currentValue;
                inNumber = false;
            }
        }

        if (inNumber)
        {
            if (!firstCaptured)
            {
                firstValue = currentValue;
            }
            lastValue = currentValue;
        }

        return sawNumber;
    }

    void clearLogicalValues(SensorsMenuValueItem (&values)[kLogicalValueCount])
    {
        for (uint8_t index = 0; index < kLogicalValueCount; ++index)
        {
            values[index] = SensorsMenuValueItem{};
        }
    }

    void setLogicalValue(SensorsMenuValueItem (&values)[kLogicalValueCount], uint8_t index, bool available, bool hasValue, int32_t value)
    {
        if (index >= kLogicalValueCount)
        {
            return;
        }

        values[index].available = available;
        values[index].hasValue = hasValue;
        values[index].value = hasValue ? value : 0;
    }

    void buildLogicalValues(const SensorsMenuCapturedEntry (&entries)[kSensorsStepCount], SensorsMenuValueItem (&values)[kLogicalValueCount])
    {
        clearLogicalValues(values);

        const SensorsMenuCapturedEntry &step1 = entries[0];
        if (step1.available)
        {
            int32_t firstValue = 0;
            int32_t lastValue = 0;
            if (parseFirstAndLastNumber(step1.rawValue, firstValue, lastValue))
            {
                setLogicalValue(values, 0, true, true, firstValue);
                setLogicalValue(values, 1, true, true, lastValue);
            }
            else
            {
                setLogicalValue(values, 0, true, false, 0);
                setLogicalValue(values, 1, true, false, 0);
            }
        }

        for (uint8_t stepIndex = 1; stepIndex < kSensorsStepCount; ++stepIndex)
        {
            const SensorsMenuCapturedEntry &entry = entries[stepIndex];
            setLogicalValue(values, stepIndex + 1U, entry.available, entry.hasValue, entry.value);
        }
    }

    void captureCurrentStepValue(uint8_t step, const char *displayText)
    {
        if (step < kFirstSensorsStep || step > kLastSensorsStep)
        {
            return;
        }

        const uint8_t stepIndex = static_cast<uint8_t>(step - 1U);
        SensorsMenuCapturedEntry &entry = g_scanState.entries[stepIndex];

        copyDisplayText(g_scanState.lastDisplayText, displayText);
        g_scanState.currentStep = step;
        entry.available = true;
        copyDisplayText(entry.rawValue, displayText);
        clearDetail(entry.detail);
        entry.hasAuxValue = false;
        entry.auxValue = 0;

        int32_t parsedValue = 0;
        entry.hasValue = parseLastNumber(displayText, parsedValue);
        entry.value = entry.hasValue ? parsedValue : 0;

        if (step == 1)
        {
            int32_t firstValue = 0;
            int32_t lastValue = 0;
            if (parseFirstAndLastNumber(displayText, firstValue, lastValue))
            {
                entry.hasValue = true;
                entry.value = firstValue;
                entry.hasAuxValue = true;
                entry.auxValue = lastValue;
            }
        }
    }

    int8_t findUnknownEntryIndex(const char *key)
    {
        for (uint8_t index = 0; index < kMaxUnknownSensors; ++index)
        {
            if (!g_scanState.unknownEntries[index].available)
            {
                continue;
            }

            if (std::strncmp(g_scanState.unknownEntries[index].key, key, sizeof(g_scanState.unknownEntries[index].key)) == 0)
            {
                return static_cast<int8_t>(index);
            }
        }

        return -1;
    }

    int8_t ensureUnknownEntryIndex(const char *key)
    {
        const int8_t existingIndex = findUnknownEntryIndex(key);
        if (existingIndex >= 0)
        {
            return existingIndex;
        }

        for (uint8_t index = 0; index < kMaxUnknownSensors; ++index)
        {
            if (g_scanState.unknownEntries[index].available)
            {
                continue;
            }

            SensorsMenuUnknownEntry &entry = g_scanState.unknownEntries[index];
            entry = SensorsMenuUnknownEntry{};
            entry.available = true;
            std::strncpy(entry.key, key, sizeof(entry.key) - 1);
            entry.key[sizeof(entry.key) - 1] = '\0';
            return static_cast<int8_t>(index);
        }

        return -1;
    }

    void captureUnknownEntryValue(const char *key, const char *displayText)
    {
        const int8_t entryIndex = ensureUnknownEntryIndex(key);
        if (entryIndex < 0)
        {
            return;
        }

        SensorsMenuUnknownEntry &entry = g_scanState.unknownEntries[static_cast<uint8_t>(entryIndex)];
        copyDisplayText(entry.rawValue, displayText);
        int32_t parsedValue = 0;
        entry.hasValue = parseLastNumber(displayText, parsedValue);
        entry.value = entry.hasValue ? parsedValue : 0;
    }

    bool captureCurrentEntry(const char *displayText)
    {
        char parsedKey[4] = {0};
        if (!parseSensorEntryKey(displayText, parsedKey))
        {
            return false;
        }

        std::strncpy(g_scanState.currentEntryKey, parsedKey, sizeof(g_scanState.currentEntryKey) - 1);
        g_scanState.currentEntryKey[sizeof(g_scanState.currentEntryKey) - 1] = '\0';
        if (g_scanState.firstEntryKey[0] == '\0')
        {
            std::strncpy(g_scanState.firstEntryKey, parsedKey, sizeof(g_scanState.firstEntryKey) - 1);
            g_scanState.firstEntryKey[sizeof(g_scanState.firstEntryKey) - 1] = '\0';
        }

        const uint8_t step = sensorStepForKey(parsedKey);
        if (step == 0)
        {
            captureUnknownEntryValue(parsedKey, displayText);
            return true;
        }

        captureCurrentStepValue(step, displayText);
        return true;
    }

    void startStep(uint32_t now, KeyMask keyMask)
    {
        g_scanState.phaseStartedMs = now;
        g_scanState.stepStarted = true;
        g_scanState.keysReleased = false;
        pressKeys(keyMask);
    }

    bool isWaitCompleted(uint32_t now, uint32_t ms)
    {
        return static_cast<uint32_t>(now - g_scanState.phaseStartedMs) >= ms;
    }

    void releaseKeys(uint32_t now)
    {
        pressKeys(kKeyNone);
        g_scanState.keysReleased = true;
        g_scanState.phaseStartedMs = now;
    }

    void advanceToNextStep()
    {
        ++g_scanState.currentScriptIndex;
        g_scanState.stepStarted = false;
    }

    void advanceToStep(uint8_t stepIndex)
    {
        g_scanState.currentScriptIndex = stepIndex;
        g_scanState.stepStarted = false;
        g_scanState.keysReleased = false;
    }

    void finishScan()
    {
        pressKeys(kKeyNone);

        portENTER_CRITICAL(&g_menuMux);
        std::memcpy(g_lastCompletedEntries, g_scanState.entries, sizeof(g_lastCompletedEntries));
        std::memcpy(g_lastCompletedUnknownEntries, g_scanState.unknownEntries, sizeof(g_lastCompletedUnknownEntries));
        copyDisplayText(g_lastCompletedDisplayText, g_scanState.lastDisplayText);
        g_lastCompletedMs = millis();
        portEXIT_CRITICAL(&g_menuMux);

        g_scanState = SensorsMenuState{};
    }

    void abortScan()
    {
        pressKeys(kKeyNone);
        g_scanState = SensorsMenuState{};
    }

    void runCurrentStep(uint32_t now)
    {
        const SensorsMenuStep &step = kReadScript[g_scanState.currentScriptIndex];
        if (!g_scanState.stepStarted)
        {
            startStep(now, step.keyPressed);
            return;
        }

        if (!g_scanState.keysReleased)
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
        copyDisplayText(g_scanState.lastDisplayText, snapshot.text);

        SensorsStepResult result = SensorsStepResult::Abort;
        switch (step.action)
        {
        case SensorsAction::ReadCurrentEntry:
            result = captureCurrentEntry(snapshot.text) ? SensorsStepResult::JumpToNextEntry : SensorsStepResult::Pending;
            break;

        case SensorsAction::SelectNextEntry:
        {
            char parsedKey[4] = {0};
            if (!parseSensorEntryKey(snapshot.text, parsedKey))
            {
                result = SensorsStepResult::Pending;
                break;
            }

            if (std::strncmp(parsedKey, g_scanState.currentEntryKey, sizeof(parsedKey)) == 0)
            {
                result = SensorsStepResult::Pending;
                break;
            }

            if (g_scanState.firstEntryKey[0] != '\0' &&
                std::strncmp(parsedKey, g_scanState.firstEntryKey, sizeof(parsedKey)) == 0)
            {
                result = SensorsStepResult::JumpToExitMenu;
                break;
            }

            result = captureCurrentEntry(snapshot.text) ? SensorsStepResult::JumpToNextEntry : SensorsStepResult::Abort;
            break;
        }

        case SensorsAction::FinishRead:
            result = SensorsStepResult::Finish;
            break;
        }

        switch (result)
        {
        case SensorsStepResult::Pending:
            if (isWaitCompleted(now, kStepDisplayTimeoutMs))
            {
                abortScan();
            }
            return;

        case SensorsStepResult::JumpToNextEntry:
            advanceToStep(kNextEntryStepIndex);
            return;

        case SensorsStepResult::JumpToExitMenu:
            advanceToStep(kExitMenuStepIndex);
            return;

        case SensorsStepResult::Finish:
            finishScan();
            return;

        case SensorsStepResult::Abort:
            abortScan();
            return;
        }
    }
} // namespace

void sensorsMenuSetup()
{
    stopSensorsMenuScan();
    g_lastScanStartedMs = millis();
    g_startupScanPending = autoScanAllowed();

    if (!autoScanAllowed())
    {
        return;
    }
}

void sensorsMenuLoop()
{
    const uint32_t now = millis();

    if (!g_scanState.running)
    {
        if (g_startupScanPending)
        {
            startSensorsMenuScan();
        }
        else if (autoScanAllowed() && static_cast<uint32_t>(now - g_lastScanStartedMs) >= kAutoScanIntervalMs)
        {
            startSensorsMenuScan();
        }
        return;
    }

    runCurrentStep(now);
}

void startSensorsMenuScan()
{
    if (g_scanState.running || settingsMenuIsBusy() || settingWriterIsBusy())
    {
        return;
    }

    g_lastScanStartedMs = millis();
    g_startupScanPending = false;
    g_scanState.running = true;
}

void stopSensorsMenuScan()
{
    pressKeys(kKeyNone);
    g_scanState = SensorsMenuState{};
}

bool sensorsMenuIsBusy()
{
    return g_scanState.running;
}

bool sensorsMenuAutoScanEnabled()
{
    return autoScanAllowed();
}

SensorsMenuStatus getSensorsMenuStatus()
{
    SensorsMenuStatus status{};

    portENTER_CRITICAL(&g_menuMux);
    status.running = g_scanState.running;
    status.done = g_scanState.done;
    status.currentStep = g_scanState.currentStep;
    status.lastCompletedMs = g_lastCompletedMs;
    std::strncpy(status.phase, currentPhaseName(), sizeof(status.phase) - 1);
    status.phase[sizeof(status.phase) - 1] = '\0';
    copyDisplayText(status.lastDisplayText,
                    g_scanState.running ? g_scanState.lastDisplayText : g_lastCompletedDisplayText);

    for (uint8_t index = 0; index < kSensorsStepCount; ++index)
    {
        status.entries[index] = g_lastCompletedEntries[index];
        if (g_scanState.running && g_scanState.entries[index].available)
        {
            status.entries[index] = g_scanState.entries[index];
        }
    }
    for (uint8_t index = 0; index < kMaxUnknownSensors; ++index)
    {
        status.unknownEntries[index] = g_lastCompletedUnknownEntries[index];
        if (g_scanState.running && g_scanState.unknownEntries[index].available)
        {
            status.unknownEntries[index] = g_scanState.unknownEntries[index];
        }
    }
    buildLogicalValues(status.entries, status.values);
    portEXIT_CRITICAL(&g_menuMux);

    return status;
}

SensorsMenuDefinition getSensorsMenuDefinition(uint8_t step)
{
    if (step < kFirstSensorsStep || step > kLastSensorsStep)
    {
        return {"", "", ""};
    }

    return kMenuDefinitions[step - 1U];
}

SensorsMenuValueDefinition getSensorsMenuValueDefinition(uint8_t index)
{
    if (index == 0 || index > kLogicalValueCount)
    {
        return {"", "", "", ""};
    }

    return kValueDefinitions[index - 1U];
}