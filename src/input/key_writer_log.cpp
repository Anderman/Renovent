#include "input/key_writer_log.h"

#include <cstring>

#include "input/keypad.h"

namespace
{
    constexpr uint16_t kMaxLogEntries = 400;

    KeyPressLogEntry g_logEntries[kMaxLogEntries] = {};
    uint16_t g_logNextIndex = 0;
    uint16_t g_logCount = 0;
    char g_lastDisplayText[9] = {0};

    void copyKeyText(char (&destination)[24], uint8_t mask)
    {
        const String text = activeKeysToString(mask);
        std::strncpy(destination, text.c_str(), sizeof(destination) - 1);
        destination[sizeof(destination) - 1] = '\0';
    }

    void copyDisplayText(char (&destination)[9], const char *source)
    {
        if (source == nullptr)
        {
            destination[0] = '\0';
            return;
        }

        std::strncpy(destination, source, sizeof(destination) - 1);
        destination[sizeof(destination) - 1] = '\0';
    }

    void appendLogEntry(const char *eventName,
                        uint8_t mask,
                        uint32_t relativeMs,
                        uint32_t idleBeforeMs,
                        uint32_t releaseForMs,
                        const char *displayText)
    {
        KeyPressLogEntry &entry = g_logEntries[g_logNextIndex];
        entry.available = true;
        std::strncpy(entry.event, eventName, sizeof(entry.event) - 1);
        entry.event[sizeof(entry.event) - 1] = '\0';
        entry.mask = mask;
        entry.relativeMs = relativeMs;
        entry.idleBeforeMs = idleBeforeMs;
        entry.releaseForMs = releaseForMs;
        copyKeyText(entry.keys, mask);
        copyDisplayText(entry.display, displayText);

        g_logNextIndex = static_cast<uint16_t>((g_logNextIndex + 1U) % kMaxLogEntries);
        if (g_logCount < kMaxLogEntries)
        {
            ++g_logCount;
        }
    }
} // namespace

void keyWriterLogPress(uint8_t mask, uint32_t idleBeforeMs)
{
    appendLogEntry("press", mask, 0, idleBeforeMs, 0, g_lastDisplayText);
}

void keyWriterLogRelease(uint8_t mask, uint32_t relativeMs)
{
    appendLogEntry("release", mask, relativeMs, 0, 0, g_lastDisplayText);
}

void keyWriterLogDisplayChanged(uint8_t activeMask, uint32_t relativeMs, uint32_t releaseForMs, const char *displayText)
{
    if (displayText == nullptr || std::strncmp(g_lastDisplayText, displayText, sizeof(g_lastDisplayText)) == 0)
    {
        return;
    }

    copyDisplayText(g_lastDisplayText, displayText);
    appendLogEntry("display", activeMask, relativeMs, 0, releaseForMs, g_lastDisplayText);
}

uint16_t keyWriterLogCopyEntries(KeyPressLogEntry *entries, uint16_t maxEntries)
{
    if (entries == nullptr || maxEntries == 0)
    {
        return 0;
    }

    const uint16_t copiedCount = g_logCount < maxEntries ? g_logCount : maxEntries;
    for (uint16_t index = 0; index < copiedCount; ++index)
    {
        const uint16_t sourceIndex = static_cast<uint16_t>((g_logNextIndex + kMaxLogEntries - 1U - index) % kMaxLogEntries);
        entries[index] = g_logEntries[sourceIndex];
    }

    return copiedCount;
}
