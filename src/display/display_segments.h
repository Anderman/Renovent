#pragma once

#include <Arduino.h>

namespace display_segments {
constexpr uint8_t kSegmentA = 1U << 0;  // 0b00000001
constexpr uint8_t kSegmentB = 1U << 1;  // 0b00000010
constexpr uint8_t kSegmentC = 1U << 2;  // 0b00000100
constexpr uint8_t kSegmentD = 1U << 3;  // 0b00001000
constexpr uint8_t kSegmentE = 1U << 4;  // 0b00010000
constexpr uint8_t kSegmentF = 1U << 5;  // 0b00100000
constexpr uint8_t kSegmentG = 1U << 6;  // 0b01000000
constexpr uint8_t kDecimalPoint = 1U << 7;  // 0b10000000
}