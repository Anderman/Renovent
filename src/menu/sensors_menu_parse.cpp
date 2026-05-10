#include "menu/sensors_menu_internal.h"

#include <cstring>

#include "display/display_text_utils.h"

namespace sensors_menu_internal {
namespace {

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
    if (std::strcmp(key, "P1") == 0)
    {
        return 7;
    }
    if (std::strcmp(key, "P2") == 0)
    {
        return 8;
    }
    if (std::strcmp(key, "N") == 0)
    {
        return 9;
    }
    if (std::strcmp(key, "U") == 0)
    {
        return 10;
    }
    if (std::strcmp(key, "T") == 0)
    {
        return 11;
    }
    if (std::strcmp(key, "A") == 0)
    {
        return 12;
    }
    if (std::strcmp(key, "U0") == 0)
    {
        return 13;
    }
    if (std::strcmp(key, "ST") == 0)
    {
        return 14;
    }
    if (std::strcmp(key, "PT") == 0)
    {
        return 15;
    }
    if (std::strcmp(key, "TN") == 0)
    {
        return 16;
    }

    return 0;
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

void captureCurrentStepValue(uint8_t step, const ParsedSensorEntry &parsedEntry)
{
    if (step < kFirstSensorsStep || step > kLastSensorsStep)
    {
        return;
    }

    const uint8_t stepIndex = static_cast<uint8_t>(step - 1U);
    SensorsMenuCapturedEntry &entry = g_scanState.entries[stepIndex];

    g_scanState.currentStep = step;
    entry.available = true;
    entry.hasAuxValue = false;
    entry.auxValue = 0;

    entry.hasValue = parsedEntry.hasValue;
    entry.value = parsedEntry.hasValue ? parsedEntry.value : 0;

    if (step == 1)
    {
        if (parsedEntry.key[0] >= '1' && parsedEntry.key[0] <= '3' && parsedEntry.key[1] == '\0')
        {
            entry.hasValue = true;
            entry.value = parsedEntry.key[0] - '0';
            entry.hasAuxValue = parsedEntry.hasValue;
            entry.auxValue = parsedEntry.hasValue ? parsedEntry.value : 0;
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

void captureUnknownEntryValue(const char *key, const char *displayText, const ParsedSensorEntry &parsedEntry)
{
    const int8_t entryIndex = ensureUnknownEntryIndex(key);
    if (entryIndex < 0)
    {
        return;
    }

    SensorsMenuUnknownEntry &entry = g_scanState.unknownEntries[static_cast<uint8_t>(entryIndex)];
    copyDisplayText(entry.rawValue, displayText);
    entry.hasValue = parsedEntry.hasValue;
    entry.value = parsedEntry.hasValue ? parsedEntry.value : 0;
}

} // namespace

bool captureCurrentEntry(const char *displayText, const ParsedSensorEntry &parsedEntry)
{
    if (parsedEntry.key[0] == '\0')
    {
        return false;
    }

    std::strncpy(g_scanState.currentEntryKey, parsedEntry.key, sizeof(g_scanState.currentEntryKey) - 1);
    g_scanState.currentEntryKey[sizeof(g_scanState.currentEntryKey) - 1] = '\0';
    if (g_scanState.firstEntryKey[0] == '\0')
    {
        std::strncpy(g_scanState.firstEntryKey, parsedEntry.key, sizeof(g_scanState.firstEntryKey) - 1);
        g_scanState.firstEntryKey[sizeof(g_scanState.firstEntryKey) - 1] = '\0';
    }

    const uint8_t step = sensorStepForKey(parsedEntry.key);
    if (step == 0)
    {
        captureUnknownEntryValue(parsedEntry.key, displayText, parsedEntry);
        return true;
    }

    captureCurrentStepValue(step, parsedEntry);
    return true;
}

bool captureCurrentEntry(const char *displayText)
{
    ParsedSensorEntry parsedEntry{};
    if (!parseSensorEntry(displayText, parsedEntry))
    {
        return false;
    }

    return captureCurrentEntry(displayText, parsedEntry);
}

void buildLogicalValues(const SensorsMenuCapturedEntry (&entries)[kSensorsStepCount], SensorsMenuValueItem (&values)[kLogicalValueCount])
{
    clearLogicalValues(values);

    const SensorsMenuCapturedEntry &step1 = entries[0];
    if (step1.available)
    {
        setLogicalValue(values, 0, true, step1.hasValue, step1.value);
        setLogicalValue(values, 1, true, step1.hasAuxValue, step1.auxValue);
    }

    for (uint8_t stepIndex = 1; stepIndex < kSensorsStepCount; ++stepIndex)
    {
        const SensorsMenuCapturedEntry &entry = entries[stepIndex];
        setLogicalValue(values, stepIndex + 1U, entry.available, entry.hasValue, entry.value);
    }
}

} // namespace sensors_menu_internal
