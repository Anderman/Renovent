#pragma once

#include <Arduino.h>

#include "display/display_text_utils.h"
#include "menu/sensors_menu.h"

namespace sensors_menu_internal {

constexpr uint8_t kMaxUnknownSensors = 8;
constexpr uint8_t kFirstSensorsStep = 1;
constexpr uint8_t kLastSensorsStep = 16;
constexpr uint8_t kSensorsStepCount = kLastSensorsStep - kFirstSensorsStep + 1U;
constexpr uint8_t kLogicalValueCount = 17;
constexpr uint32_t kAutoScanIntervalMs = 60000;

struct SensorsMenuState
{
    bool running = false;
    uint8_t currentStep = 0;
    uint8_t currentScriptIndex = 0;
    uint32_t phaseStartedMs = 0;
    bool stepStarted = false;
    bool keysReleased = false;
    char firstEntryKey[4] = {0};
    char currentEntryKey[4] = {0};
    SensorsMenuCapturedEntry entries[kSensorsStepCount] = {};
    SensorsMenuUnknownEntry unknownEntries[kMaxUnknownSensors] = {};
};

extern portMUX_TYPE g_menuMux;
extern SensorsMenuState g_scanState;
extern SensorsMenuCapturedEntry g_lastCompletedEntries[kSensorsStepCount];
extern SensorsMenuUnknownEntry g_lastCompletedUnknownEntries[kMaxUnknownSensors];
extern uint32_t g_lastCompletedMs;
extern uint32_t g_lastScanStartedMs;

bool captureCurrentEntry(const char *displayText, const ParsedSensorEntry &parsedEntry);
bool captureCurrentEntry(const char *displayText);
void buildLogicalValues(const SensorsMenuCapturedEntry (&entries)[kSensorsStepCount], SensorsMenuValueItem (&values)[kLogicalValueCount]);
void runCurrentStep(uint32_t now);

} // namespace sensors_menu_internal
