#pragma once

#include <Arduino.h>

enum KeyMask : uint8_t {
  kKeyNone = 0,
  kKeyOk = 1 << 0,
  kKeyPlus = 1 << 1,
  kKeyFunction = 1 << 2,
  kKeyMinus = 1 << 3,
};