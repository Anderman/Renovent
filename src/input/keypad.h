#pragma once

#include <Arduino.h>

#include "input/input_keys.h"

void keypadSetup();
uint8_t keypadGetActiveKeys();
String activeKeysToString(uint8_t activeKeys);