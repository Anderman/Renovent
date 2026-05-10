#pragma once

#include <Arduino.h>

void keyWriterApplySelectIndexHook(uint8_t selectIndex);
void keyWriterOnDisplayChangedHook(const char *displayText);