#include "menu/sensors_menu.h"

#include <cstring>

#include "core/app_config.h"
#include "display/display_reader.h"
#include "display/display_text_utils.h"
#include "input/key_writer.h"
#include "menu/setting_writer.h"
#include "menu/settings_menu.h"
#include "menu/sensors_menu_internal.h"

namespace
{

    constexpr SensorsMenuDefinition kMenuDefinitions[sensors_menu_internal::kSensorsStepCount] = {
        {"2.200", "Actuele stand/afvoervolume", "Standenschakelaar 1, 2 of 3 plus ingesteld afvoervolume [m3/h]."},
        {"C 0", "Meldcode bedrijfssituatie", "C0 geen melding, C3/C6 constant druk, C7 correctie maximale luchtvolume."},
        {"bP.1", "Status bypass", "Alleen indien gemonteerd: 0 dicht, 1 open, 2 toevoer minimaal."},
        {"tP.9", "Temperatuur van buiten", "Bij negatieve temperatuur kan de uitlezing afwijken van het voorbeeld."},
        {"ts.21", "Temperatuur van binnen", "Temperatuurweergave in graden Celsius."},
        {"In.1", "Toestel geinitialiseerd", "In=0 nee, In=1 ja."},
        {"P1.6.2", "Voltage op proportionele ingang 1", "Alleen met optieprint: spanning op ingang P1 [V]."},
        {"P2.5.2", "Voltage op proportionele ingang 2", "Alleen met optieprint: spanning op ingang P2 [V]."},
        {"u.156", "Actueel toevoervolume", "Actueel toevoervolume [m3/h]."},
        {"u.156", "Actueel afvoervolume", "Actueel afvoervolume [m3/h]."},
        {"t.180", "Actuele druk toevoerkanaal", "Actuele druk [Pa]."},
        {"A.180", "Actuele druk afvoerkanaal", "Actuele druk [Pa]."},
        {"u0.0", "Status vorstbeveiliging", "0 niet, 1 t/m 4 onbalans, 5 toevoerventilator uit."},
        {"st.9", "Temperatuur naar buiten", "Wanneer voeler niet is aangesloten toont de unit een vaste fallbackwaarde."},
        {"Pt.18", "Temperatuur naar binnen", "Wanneer voeler niet is aangesloten toont de unit een vaste fallbackwaarde."},
        {"tn.25", "Temperatuur naverwarmer", "Alleen met optieprint; 0 betekent niet actief."},
    };

    constexpr SensorsMenuValueDefinition kValueDefinitions[sensors_menu_internal::kLogicalValueCount] = {
        {"fan_mode", "Actuele stand", "", "Standenschakelaar 1, 2 of 3.", 0},
        {"exhaust_setpoint", "Ingesteld afvoervolume", "m3/h", "Ingesteld afvoervolume [m3/h].", 0},
        {"operation_code", "Meldcode bedrijfssituatie", "", "C0 geen melding, C3/C6 constant druk, C7 correctie maximale luchtvolume.", 0},
        {"bypass_status", "Status bypass", "", "0 dicht, 1 open, 2 toevoer minimaal.", 0},
        {"outside_temperature", "Temperatuur van buiten", "C", "Temperatuur van buiten in graden Celsius.", 0},
        {"inside_temperature", "Temperatuur van binnen", "C", "Temperatuur van binnen in graden Celsius.", 0},
        {"input_status", "Toestel geinitialiseerd", "", "In=0 nee, In=1 ja.", 0},
        {"proportional_input_1_voltage", "Voltage op proportionele ingang 1", "V", "Alleen met optieprint; spanning op ingang P1.", 1},
        {"proportional_input_2_voltage", "Voltage op proportionele ingang 2", "V", "Alleen met optieprint; spanning op ingang P2.", 1},
        {"supply_flow", "Actueel toevoervolume", "m3/h", "Actueel toevoervolume [m3/h].", 0},
        {"exhaust_flow", "Actueel afvoervolume", "m3/h", "Actueel afvoervolume [m3/h].", 0},
        {"supply_pressure", "Actuele druk toevoerkanaal", "Pa", "Actuele druk [Pa].", 0},
        {"exhaust_pressure", "Actuele druk afvoerkanaal", "Pa", "Actuele druk [Pa].", 0},
        {"frost_protection", "Status vorstbeveiliging", "", "0 niet, 1 t/m 4 onbalans, 5 toevoerventilator uit.", 0},
        {"outgoing_temperature", "Temperatuur naar buiten", "C", "Fallbackwaarde als voeler niet is aangesloten.", 0},
        {"incoming_temperature", "Temperatuur naar binnen", "C", "Fallbackwaarde als voeler niet is aangesloten.", 0},
        {"reheater_temperature", "Temperatuur naverwarmer", "C", "Alleen met optieprint; 0 betekent niet actief.", 0},
    };

} // namespace

namespace sensors_menu_internal
{
    portMUX_TYPE g_menuMux = portMUX_INITIALIZER_UNLOCKED;
    SensorsMenuState g_scanState;
    SensorsMenuCapturedEntry g_lastCompletedEntries[kSensorsStepCount] = {};
    SensorsMenuUnknownEntry g_lastCompletedUnknownEntries[kMaxUnknownSensors] = {};
    char g_lastCompletedDisplayText[9] = {0};
    uint32_t g_lastCompletedMs = 0;
    uint32_t g_lastScanStartedMs = 0;

    bool isValidSensorsStartDisplay(const char *displayText)
    {
        return startsWithDisplay(displayText, "0.") ||
               startsWithDisplay(displayText, "1.") ||
               startsWithDisplay(displayText, "2.") ||
               startsWithDisplay(displayText, "3.");
    }

} // namespace sensors_menu_internal

void startSensorsMenuScan()
{
    using namespace sensors_menu_internal;
    g_lastScanStartedMs = millis();
    g_scanState.running = true;
}

bool canStartSensorsMenu()
{
    using namespace sensors_menu_internal;

    if (g_scanState.running || settingsMenuIsBusy() || settingWriterIsBusy())
    {
        return false;
    }

    const DisplaySnapshot snapshot = getDisplaySnapshot();
    return isValidSensorsStartDisplay(snapshot.text);
}

void sensorsMenuSetup()
{
    using namespace sensors_menu_internal;

    const uint32_t now = millis();
    g_lastScanStartedMs = now - kAutoScanIntervalMs;
}

void sensorsMenuLoop()
{
    using namespace sensors_menu_internal;

    const uint32_t now = millis();
    if (!g_scanState.running)
    {
        if (canStartSensorsMenu() && static_cast<uint32_t>(now - g_lastScanStartedMs) >= kAutoScanIntervalMs)
        {
            startSensorsMenuScan();
        }
        return;
    }

    runCurrentStep(now);
}

bool sensorsMenuIsBusy()
{
    return sensors_menu_internal::g_scanState.running;
}

SensorsMenuProgress getSensorsMenuProgress()
{
    using namespace sensors_menu_internal;

    SensorsMenuProgress progress{};

    portENTER_CRITICAL(&g_menuMux);
    progress.running = g_scanState.running;
    progress.currentStep = g_scanState.currentStep;
    progress.lastCompletedMs = g_lastCompletedMs;
    std::strncpy(progress.phase, currentPhaseName(), sizeof(progress.phase) - 1);
    progress.phase[sizeof(progress.phase) - 1] = '\0';
    copyDisplayText(progress.lastDisplayText,
                    g_scanState.running ? g_scanState.lastDisplayText : g_lastCompletedDisplayText);
    portEXIT_CRITICAL(&g_menuMux);

    return progress;
}

SensorsMenuSnapshot getSensorsMenuSnapshot()
{
    using namespace sensors_menu_internal;

    SensorsMenuSnapshot snapshot{};

    portENTER_CRITICAL(&g_menuMux);
    snapshot.lastCompletedMs = g_lastCompletedMs;

    for (uint8_t index = 0; index < kSensorsStepCount; ++index)
    {
        snapshot.entries[index] = g_lastCompletedEntries[index];
    }
    for (uint8_t index = 0; index < kMaxUnknownSensors; ++index)
    {
        snapshot.unknownEntries[index] = g_lastCompletedUnknownEntries[index];
    }
    portEXIT_CRITICAL(&g_menuMux);

    buildLogicalValues(snapshot.entries, snapshot.values);

    return snapshot;
}

SensorsMenuDefinition getSensorsMenuDefinition(uint8_t step)
{
    if (step < sensors_menu_internal::kFirstSensorsStep || step > sensors_menu_internal::kLastSensorsStep)
    {
        return {"", "", ""};
    }

    return kMenuDefinitions[step - 1U];
}

SensorsMenuValueDefinition getSensorsMenuValueDefinition(uint8_t index)
{
    if (index == 0 || index > sensors_menu_internal::kLogicalValueCount)
    {
        return {"", "", "", "", 0};
    }

    return kValueDefinitions[index - 1U];
}