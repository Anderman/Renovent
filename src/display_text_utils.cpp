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

bool parseLastNumber(const char *rawValue, int32_t &value)
{
    int32_t currentValue = 0;
    bool inNumber = false;
    bool sawNumber = false;

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
            value = currentValue;
            inNumber = false;
        }
    }

    if (inNumber)
    {
        value = currentValue;
    }

    return sawNumber;
}