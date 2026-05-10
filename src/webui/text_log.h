#pragma once

#include <Arduino.h>

struct TextLogEntry
{
    bool available = false;
    uint32_t timestampMs = 0;
    char message[128] = {0};
};

void textLogAdd(const char *message);
void textLogAddf(const char *format, ...);
uint16_t textLogCopyEntries(TextLogEntry *entries, uint16_t maxEntries);