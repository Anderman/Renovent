#pragma once

#include <Arduino.h>

struct DisplaySnapshot {
  char text[9];
};

void displayReaderSetup();
void displayReaderLoop();
DisplaySnapshot getDisplaySnapshot();
