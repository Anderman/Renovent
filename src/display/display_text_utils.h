#pragma once

#include <Arduino.h>

struct ParsedSettingValue
{
	bool isValid = false;
	char displayValue[9] = {0};
	bool hasNumericValue = false;
	int32_t numericValue = 0;
};

struct ParsedSensorEntry
{
	char key[4] = {0};
	const char *valueText = nullptr;
	bool hasValue = false;
	int32_t value = 0;
};

void copyDisplayText(char (&destination)[9], const char *source);
bool tryGetCompactSettingText(const char *source, char (&value)[9]);
bool isStartDisplay(const char *displayText);
bool startsWithDisplay(const char *actual, const char *expectedPrefix);
bool tryGetNumericValue(const char *rawValue, int32_t &value);
bool tryParseSensorEntry(const char *displayText, ParsedSensorEntry &parsedEntry);
bool tryGetSensorKey(const char *displayText, char (&key)[4], const char *&valueStart);
bool tryGetSensorKey(const char *displayText, char (&key)[4]);
bool tryGetSettingKey(const char *displayText, char (&key)[4]);
bool tryGetDisplaySettingValue(const char *displayText, ParsedSettingValue &value);
bool tryGetInputSettingValue(const char *inputText, ParsedSettingValue &value);