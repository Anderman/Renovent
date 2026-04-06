#pragma once

#include <Arduino.h>

#include "input_keys.h"

void keypadSetup();
void keypadOnStableIndex(uint8_t selectIndex, bool keyPressed);
void keypadCommitActiveKeys(uint8_t activeKeys);
uint8_t getActiveKeys();
String activeKeysToString(uint8_t activeKeys);