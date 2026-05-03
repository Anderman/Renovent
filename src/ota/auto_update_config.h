#pragma once

namespace autoUpdateConfig {
constexpr bool kEnabled = true;
constexpr char kGitHubOwner[] = "Anderman";
constexpr char kGitHubRepo[] = "Renovent";
constexpr char kGitHubRef[] = "main";
constexpr char kUserAgent[] = "renovent-updater";
constexpr unsigned long kInitialCheckDelayMs = 60UL * 1000UL;
constexpr unsigned long kCheckIntervalMs = 60UL * 1000UL;
}