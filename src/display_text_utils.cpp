#include "display_text_utils.h"

#include <cstring>

void copyDisplayText(char (&destination)[9], const char *source)
{
    std::strncpy(destination, source, sizeof(destination) - 1);
    destination[sizeof(destination) - 1] = '\0';
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

bool parseDisplayKey(const char *displayText, char (&key)[4])
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
            continue;
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
// Parses the last numeric token from the display into the internal fixed-point
// integer representation used by the firmware.
// Examples with 1 decimal precision on the display:
// .5   -> 5
// 5.   -> -5
// -10.5. -> -105
// - 1.5. -> -15
bool parseLastNumber(const char *rawValue, int32_t &value)
{
    if (rawValue == nullptr)
    {
        return false;
    }

    char compact[16] = {0};
    uint8_t compactLength = 0;
    for (uint8_t index = 0; rawValue[index] != '\0' && compactLength < sizeof(compact) - 1U; ++index)
    {
        const char current = rawValue[index];
        if (current == ' ')
        {
            continue;
        }

        compact[compactLength++] = current;
    }
    compact[compactLength] = '\0';

    int8_t tokenEnd = -1;
    for (int8_t index = static_cast<int8_t>(compactLength) - 1; index >= 0; --index)
    {
        const char current = compact[index];
        if ((current >= '0' && current <= '9') || current == '.' || current == '-')
        {
            tokenEnd = index;
            break;
        }
    }

    if (tokenEnd < 0)
    {
        return false;
    }

    int8_t tokenStart = tokenEnd;
    while (tokenStart > 0)
    {
        const char current = compact[tokenStart - 1];
        if ((current >= '0' && current <= '9') || current == '.' || current == '-')
        {
            --tokenStart;
            continue;
        }

        break;
    }

    bool negative = false;
    int32_t parsedValue = 0;
    bool sawDigit = false;
    for (int8_t index = tokenStart; index <= tokenEnd; ++index)
    {
        const char current = compact[index];
        if (current >= '0' && current <= '9')
        {
            parsedValue = parsedValue * 10 + (current - '0');
            sawDigit = true;
            continue;
        }

        if (current == '-')
        {
            negative = true;
        }
    }

    if (!sawDigit)
    {
        return false;
    }

    if (compact[tokenEnd] == '.')
    {
        negative = true;
    }

    value = negative ? -parsedValue : parsedValue;
    return true;
}