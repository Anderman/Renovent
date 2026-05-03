#pragma once

namespace autoUpdateConfig {
constexpr bool kEnabled = true;
constexpr char kManifestUrl[] = "https://raw.githubusercontent.com/Anderman/Renovent/main/release/latest.json";
constexpr char kUserAgent[] = "renovent-updater";
constexpr unsigned long kInitialCheckDelayMs = 60UL * 1000UL;
constexpr unsigned long kCheckIntervalMs = 60UL * 1000UL;
}