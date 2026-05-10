#include "display_text_utils.h"

#include <cstring>

namespace
{
    void compactSettingDisplayText(char (&destination)[9], const char *source)
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

            destination[writeIndex++] = current == ',' ? '.' : current;
        }

        destination[writeIndex] = '\0';
    }

    bool normalizeNumericDisplayValue(char (&destination)[9], const char *source)
    {
        if (source == nullptr || source[0] == '\0')
        {
            destination[0] = '\0';
            return false;
        }

        const size_t sourceLength = std::strlen(source);
        bool negative = false;
        bool sawDigit = false;
        bool sawDecimalSeparator = false;
        uint8_t writeIndex = 0;

        if (source[sourceLength - 1U] == '.')
        {
            negative = true;
        }

        for (size_t index = 0; index < sourceLength && writeIndex < sizeof(destination) - 1U; ++index)
        {
            const char current = source[index];

            if (current == '-')
            {
                negative = true;
                continue;
            }

            if (current >= '0' && current <= '9')
            {
                destination[writeIndex++] = current;
                sawDigit = true;
                continue;
            }

            if (current == '.' && index != sourceLength - 1U && !sawDecimalSeparator)
            {
                if (writeIndex == 0)
                {
                    destination[writeIndex++] = '0';
                }

                destination[writeIndex++] = '.';
                sawDecimalSeparator = true;
            }
        }

        if (!sawDigit)
        {
            destination[0] = '\0';
            return false;
        }

        destination[writeIndex] = '\0';

        if (!negative)
        {
            return true;
        }

        if (writeIndex >= sizeof(destination) - 1U)
        {
            return false;
        }

        for (int8_t index = static_cast<int8_t>(writeIndex); index >= 0; --index)
        {
            destination[index + 1] = destination[index];
        }
        destination[0] = '-';
        return true;
    }
}

void copyDisplayText(char (&destination)[9], const char *source)
{
    std::strncpy(destination, source, sizeof(destination) - 1);
    destination[sizeof(destination) - 1] = '\0';
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
bool getNumericValue(const char *rawValue, int32_t &value)
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

    if (tokenLength == 0)
    {
        return false;
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

bool parseSensorEntry(const char *displayText, ParsedSensorEntry &parsedEntry)
{
    parsedEntry = ParsedSensorEntry{};
    if (!getSensorKey(displayText, parsedEntry.key, parsedEntry.valueText))
    {
        return false;
    }

    if (parsedEntry.valueText == nullptr || parsedEntry.valueText[0] == '\0')
    {
        return false;
    }

    parsedEntry.hasValue = getNumericValue(parsedEntry.valueText, parsedEntry.value);
    return true;
}

bool getSensorKey(const char *displayText, char (&key)[4], const char *&valueStart)
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

        if (current >= 'a' && current <= 'z')
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

    while (displayText[readIndex] == ' ')
    {
        ++readIndex;
    }

    valueStart = displayText + readIndex;
    return writeIndex > 0U;
}

bool getSensorKey(const char *displayText, char (&key)[4])
{
    const char *valueStart = nullptr;
    return getSensorKey(displayText, key, valueStart);
}

bool getSettingKey(const char *displayText, char (&key)[4])
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

bool getSettingValue(const char *displayText, ParsedSettingValue &value)
{
    value = ParsedSettingValue{};

    char compactDisplayValue[9] = {0};
    compactSettingDisplayText(compactDisplayValue, displayText);

    if (compactDisplayValue[0] == '\0')
    {
        return false;
    }

    int32_t numericValue = 0;
    if (getNumericValue(compactDisplayValue, numericValue))
    {
        if (!normalizeNumericDisplayValue(value.displayValue, compactDisplayValue))
        {
            return false;
        }

        value.isValid = true;
        value.hasNumericValue = true;
        value.numericValue = numericValue;
        return true;
    }

    copyDisplayText(value.displayValue, compactDisplayValue);
    value.isValid = true;
    value.hasNumericValue = false;
    return true;
}