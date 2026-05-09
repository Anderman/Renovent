#include "sensors_menu_internal.h"

#include <cstring>

#include "display_text_utils.h"

namespace sensors_menu_internal {
namespace {

bool parseSensorValueForStep(uint8_t step, const char *displayText, int32_t &value)
{
    if (step == 7 || step == 8)
    {
        const char *firstDot = std::strchr(displayText, '.');
        if (firstDot == nullptr || firstDot[1] == '\0')
        {
            return false;
        }

        return parseLastNumber(firstDot + 1, value);
    }

    return parseLastNumber(displayText, value);
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
    entry.hasAuxValue = false;
    entry.auxValue = 0;

    int32_t parsedValue = 0;
    entry.hasValue = parseSensorValueForStep(step, displayText, parsedValue);
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

} // namespace

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

} // namespace sensors_menu_internal
