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

void keyWriterSetup();
void keyWriterLoop();
void pressKeys(uint8_t activeKeys);
void pulseKeys(uint8_t activeKeys, uint32_t holdMs);
void keyWriterOnSelectIndex(uint8_t selectIndex);
void keyWriterOnDisplayTextChanged(const char *displayText);
uint16_t getKeyPressLogCount();
bool getKeyPressLogEntryNewestFirst(uint16_t newestFirstIndex, KeyPressLogEntry &entry);
