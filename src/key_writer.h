#pragma once

#include <Arduino.h>

#include "input_keys.h"

struct KeyPressLogEntry {
	bool available;
	char event[12];
	uint8_t mask;
	uint32_t relativeMs;
	uint32_t idleBeforeMs;
	uint32_t releaseForMs;
	char keys[24];
	char display[9];
};

struct KeyPressLogSummary {
	uint8_t activeMask;
	uint32_t activeRelativeMs;
	uint32_t releaseForMs;
	char activeKeys[24];
	char lastDisplayText[9];
	uint16_t count;
};

void keyWriterSetup();
void keyWriterLoop();
void pressKeys(uint8_t activeKeys);
void keyWriterOnSelectIndex(uint8_t selectIndex);
void keyWriterOnDisplayTextChanged(const char *displayText);
KeyPressLogSummary getKeyPressLogSummary();
bool getKeyPressLogEntryNewestFirst(uint16_t newestFirstIndex, KeyPressLogEntry &entry);
