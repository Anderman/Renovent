#pragma once

namespace autoUpdateConfig {
constexpr bool kEnabled = false;
constexpr char kGitHubOwner[] = "";
constexpr char kGitHubRepo[] = "";
constexpr char kGitHubRef[] = "main";
constexpr char kUserAgent[] = "renovent-updater";
constexpr unsigned long kInitialCheckDelayMs = 60UL * 1000UL;
constexpr unsigned long kCheckIntervalMs = 7UL * 24UL * 60UL * 60UL * 1000UL;
}