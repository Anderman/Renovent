#pragma once

#include <cstdint>

namespace app_config {

constexpr bool kSensorsMenuAutoScanEnabled = true;
constexpr bool kSettingsMenuAutoScanEnabled = false;
constexpr uint32_t kMenuBackHoldMs = 3200;
constexpr uint32_t kMenuEnterHoldMs = 4000;
constexpr uint32_t kMenuExitHoldMs = 1250;

}  // namespace app_config