#include "webui/text_log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr uint16_t kMaxTextLogEntries = 400;

    TextLogEntry g_logEntries[kMaxTextLogEntries] = {};
    uint16_t g_logNextIndex = 0;
    uint16_t g_logCount = 0;

    void appendTextLogEntry(const char *message)
    {
        if (message == nullptr || message[0] == '\0')
        {
            return;
        }

        TextLogEntry &entry = g_logEntries[g_logNextIndex];
        entry.available = true;
        entry.timestampMs = millis();
        std::strncpy(entry.message, message, sizeof(entry.message) - 1U);
        entry.message[sizeof(entry.message) - 1U] = '\0';

        g_logNextIndex = static_cast<uint16_t>((g_logNextIndex + 1U) % kMaxTextLogEntries);
        if (g_logCount < kMaxTextLogEntries)
        {
            ++g_logCount;
        }
    }
}

void textLogAdd(const char *message)
{
    appendTextLogEntry(message);
}

void textLogAddf(const char *format, ...)
{
    if (format == nullptr || format[0] == '\0')
    {
        return;
    }

    char message[128] = {0};
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    appendTextLogEntry(message);
}

uint16_t textLogCopyEntries(TextLogEntry *entries, uint16_t maxEntries)
{
    if (entries == nullptr || maxEntries == 0)
    {
        return 0;
    }

    const uint16_t copiedCount = g_logCount < maxEntries ? g_logCount : maxEntries;
    for (uint16_t index = 0; index < copiedCount; ++index)
    {
        const uint16_t sourceIndex = static_cast<uint16_t>((g_logNextIndex + kMaxTextLogEntries - copiedCount + index) % kMaxTextLogEntries);
        entries[index] = g_logEntries[sourceIndex];
    }

    return copiedCount;
}