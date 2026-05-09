#pragma once

#include <Arduino.h>

void copyDisplayText(char (&destination)[9], const char *source);
bool startsWithDisplay(const char *actual, const char *expectedPrefix);
bool parseSettingKey(const char *displayText, char (&key)[4]);
bool parseLastNumber(const char *rawValue, int32_t &value);