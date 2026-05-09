#pragma once

#include <Arduino.h>

struct ParsedSettingValue
{
	bool isValid = false;
	char displayValue[9] = {0};
	bool hasNumericValue = false;
	int32_t numericValue = 0;
};

void copyDisplayText(char (&destination)[9], const char *source);
bool startsWithDisplay(const char *actual, const char *expectedPrefix);
bool parseSettingKey(const char *displayText, char (&key)[4]);
bool parseLastNumber(const char *rawValue, int32_t &value);
bool getSettingValue(const char *displayText, ParsedSettingValue &value);