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
        SensorsMenuCapturedEntry entries[13] = {};
    };

    struct SensorsMenuStep
    {
        const char *phaseName = "idle";
        KeyMask keyPressed = kKeyNone;
        uint32_t keyDownMs = 0;
        uint32_t settleMs = 0;
        const char *expectedDisplayPrefix = nullptr;
        uint8_t captureStep = 0;
    };

    constexpr uint8_t kFirstSensorsStep = 1;
    constexpr uint8_t kLastSensorsStep = 13;
    constexpr uint8_t kSensorsStepCount = kLastSensorsStep - kFirstSensorsStep + 1U;
    constexpr uint8_t kLogicalValueCount = 14;
    constexpr uint8_t kNoCaptureStep = 0;
    constexpr uint32_t kAutoScanIntervalMs = 60000;
    constexpr uint32_t kStepDisplayTimeoutMs = 2000;

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
        {"enter-step-7", static_cast<KeyMask>(kKeyFunction | kKeyOk), 3200, 100, "n.", 7},
        {"next-step-8", kKeyPlus, 150, 100, "u.", 8},
        {"next-step-9", kKeyPlus, 150, 100, "t.", 9},
        {"next-step-10", kKeyPlus, 150, 100, "A.", 10},
        {"next-step-11", kKeyPlus, 150, 100, "u0", 11},
        {"next-step-12", kKeyPlus, 150, 100, "st", 12},
        {"next-step-13", kKeyPlus, 150, 100, "Pt", 13},
        {"next-step-1", kKeyPlus, 150, 100, nullptr, 1},
        {"next-step-2", kKeyPlus, 150, 100, "C", 2},
        {"next-step-3", kKeyPlus, 150, 100, "bP", 3},
        {"next-step-4", kKeyPlus, 150, 100, "tP", 4},
        {"next-step-5", kKeyPlus, 150, 100, "ts", 5},
        {"next-step-6", kKeyPlus, 150, 100, "In", 6},
        {"enter-menu", kKeyFunction, 3200, 100, nullptr, kNoCaptureStep},
        {"exit-menu", kKeyFunction, 1000, 100, nullptr, kNoCaptureStep}
    };

    constexpr uint8_t kReadScriptStepCount = sizeof(kReadScript) / sizeof(kReadScript[0]);

    portMUX_TYPE g_menuMux = portMUX_INITIALIZER_UNLOCKED;
    SensorsMenuState g_scanState;
    SensorsMenuCapturedEntry g_lastCompletedEntries[13] = {};
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

    void finishScan()
    {
        pressKeys(kKeyNone);

        portENTER_CRITICAL(&g_menuMux);
        std::memcpy(g_lastCompletedEntries, g_scanState.entries, sizeof(g_lastCompletedEntries));
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
        if (!startsWithDisplay(snapshot.text, step.expectedDisplayPrefix))
        {
            if (isWaitCompleted(now, kStepDisplayTimeoutMs))
            {
                abortScan();
            }
            return;
        }

        if (step.captureStep != kNoCaptureStep)
        {
            captureCurrentStepValue(step.captureStep, snapshot.text);
        }

        if (g_scanState.currentScriptIndex == kReadScriptStepCount - 1U)
        {
            finishScan();
            return;
        }

        advanceToNextStep();
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