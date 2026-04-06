#pragma once

#include <Arduino.h>

#include "input_keys.h"

struct DisplaySnapshot {
  uint8_t activeKeys;
  char text[9];
};

struct DisplayReaderStats {
  uint32_t completeFrameCount;
  uint32_t missedSelectFrameCount;
  uint32_t publishedFrameCount;
  uint8_t missedSelectPercent;
};

void displayReaderSetup();
void displayReaderLoop();
DisplaySnapshot getDisplaySnapshot();
DisplayReaderStats getDisplayReaderStats();
void getDisplayDigitMasks(uint8_t (&digitMasks)[4]);
