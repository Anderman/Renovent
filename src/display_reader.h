#pragma once

#include <Arduino.h>

#include "input_keys.h"

struct DisplaySnapshot {
  uint8_t activeKeys;
  char text[9];
};

void displayReaderSetup();
void displayReaderLoop();
DisplaySnapshot getDisplaySnapshot();
