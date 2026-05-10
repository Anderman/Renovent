#include "display_text_utils.h"

#include <cstring>

namespace
{
    void compactSettingText(char (&destination)[9], const char *source)
    {
        if (source == nullptr)
        {
            destination[0] = '\0';
            return;
        }

        uint8_t writeIndex = 0;
        for (uint8_t readIndex = 0; source[readIndex] != '\0' && writeIndex < sizeof(destination) - 1U; ++readIndex)
        {
            const char current = source[readIndex];
            if (current == ' ')
            {
                continue;
            }

            destination[writeIndex++] = current;
        }

        destination[writeIndex] = '\0';
    }

    bool parseSettingValue(const char *source, ParsedSettingValue &value)
    {
        value = ParsedSettingValue{};

        char compactValue[9] = {0};
        if (!tryGetCompactSettingText(source, compactValue))
        {
            return false;
        }

        int32_t numericValue = 0;
        if (tryGetNumericValue(compactValue, numericValue))
        {
            value.isValid = true;
            value.hasNumericValue = true;
            value.numericValue = numericValue;
            return true;
        }

        value.isValid = true;
        copyDisplayText(value.displayValue, compactValue);
        value.hasNumericValue = false;
        return true;
    }
}

void copyDisplayText(char (&destination)[9], const char *source)
{
    std::strncpy(destination, source, sizeof(destination) - 1);
    destination[sizeof(destination) - 1] = '\0';
}

bool tryGetCompactSettingText(const char *source, char (&value)[9])
{
    compactSettingText(value, source);
    return value[0] != '\0';
}

bool isStartDisplay(const char *displayText)
{
    return startsWithDisplay(displayText, "0.") ||
           startsWithDisplay(displayText, "1.") ||
           startsWithDisplay(displayText, "2.") ||
           startsWithDisplay(displayText, "3.");
}

bool startsWithDisplay(const char *actual, const char *expectedPrefix)
{
    if (expectedPrefix == nullptr || expectedPrefix[0] == '\0')
    {
        return true;
    }

    uint8_t actualIndex = 0;
    uint8_t expectedIndex = 0;

    while (expectedPrefix[expectedIndex] != '\0')
    {
        while (expectedPrefix[expectedIndex] == ' ')
        {
            ++expectedIndex;
        }

        while (actual[actualIndex] == ' ')
        {
            ++actualIndex;
        }

        if (expectedPrefix[expectedIndex] == '\0')
        {
            break;
        }

        if (actual[actualIndex] == '\0' || actual[actualIndex] != expectedPrefix[expectedIndex])
        {
            return false;
        }

        ++actualIndex;
        ++expectedIndex;
    }

    return true;
}

// Parses the last numeric token from the display into the internal fixed-point
// integer representation used by the firmware.
// Examples with 1 decimal precision on the display:
// .5   -> 5
// 5.   -> -5
// -10.5. -> -105
// - 1.5. -> -15
bool tryGetNumericValue(const char *rawValue, int32_t &value)
{
    if (rawValue == nullptr)
    {
        return false;
    }

    char token[16] = {0};
    uint8_t tokenLength = 0;

    for (uint8_t index = 0; rawValue[index] != '\0'; ++index)
    {
        const char current = rawValue[index];
        const bool isTokenChar = (current >= '0' && current <= '9') || current == '-' || current == '.';

        if (!isTokenChar)
        {
            continue;
        }

        if (tokenLength < sizeof(token) - 1U)
        {
            token[tokenLength++] = current;
        }
    }

    int32_t parsedValue = 0;
    bool sawDigit = false;
    for (uint8_t index = 0; index < tokenLength; ++index)
    {
        const char current = token[index];
        if (current < '0' || current > '9')
        {
            continue;
        }

        parsedValue = parsedValue * 10 + (current - '0');
        sawDigit = true;
    }

    if (!sawDigit)
    {
        return false;
    }

    const bool negative = token[0] == '-' || token[tokenLength - 1U] == '.';
    value = negative ? -parsedValue : parsedValue;
    return true;
}

bool tryParseSensorEntry(const char *displayText, ParsedSensorEntry &parsedEntry)
{
    parsedEntry = ParsedSensorEntry{};
    if (!tryGetSensorKey(displayText, parsedEntry.key, parsedEntry.valueText))
    {
        return false;
    }

    if (parsedEntry.valueText == nullptr || parsedEntry.valueText[0] == '\0')
    {
        return false;
    }

    parsedEntry.hasValue = tryGetNumericValue(parsedEntry.valueText, parsedEntry.value);
    return true;
}

bool tryGetSensorKey(const char *displayText, char (&key)[4], const char *&valueStart)
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

        const char upper = static_cast<char>(current & 0xDF);
        if (upper >= 'A' && upper <= 'Z')
        {
            key[writeIndex++] = upper;
            continue;
        }

        if (current >= '0' && current <= '9')
        {
            key[writeIndex++] = current;
            continue;
        }

        break;
    }

    key[writeIndex] = '\0';

    while (displayText[readIndex] == ' ')
    {
        ++readIndex;
    }

    valueStart = displayText + readIndex;
    return writeIndex > 0U;
}

bool tryGetSensorKey(const char *displayText, char (&key)[4])
{
    const char *valueStart = nullptr;
    return tryGetSensorKey(displayText, key, valueStart);
}

bool tryGetSettingKey(const char *displayText, char (&key)[4])
{
    if (displayText == nullptr)
    {
        return false;
    }

    uint8_t writeIndex = 0;
    for (uint8_t readIndex = 0; displayText[readIndex] != '\0' && writeIndex < sizeof(key) - 1U; ++readIndex)
    {
        const char current = displayText[readIndex];

        if ((current >= 'A' && current <= 'Z') || (current >= '0' && current <= '9'))
        {
            key[writeIndex++] = current;
        }
    }

    key[writeIndex] = '\0';
    return writeIndex > 0U;
}

bool tryGetDisplaySettingValue(const char *displayText, ParsedSettingValue &value)
{
    return parseSettingValue(displayText, value);
    
}

bool tryGetInputSettingValue(const char *inputText, ParsedSettingValue &value)
{
    return parseSettingValue(inputText, value);
}