#pragma once

#include <Arduino.h>

#include "input/key_writer.h"

void keyWriterLogPress(uint8_t mask, uint32_t idleBeforeMs);
void keyWriterLogRelease(uint8_t mask, uint32_t relativeMs);
void keyWriterLogDisplayChanged(uint8_t activeMask,
                                uint32_t relativeMs,
                                uint32_t releaseForMs,
                                const char *displayText);
uint16_t keyWriterLogCopyEntries(KeyPressLogEntry *entries, uint16_t maxEntries);
