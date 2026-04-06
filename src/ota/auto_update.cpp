#include "ota/auto_update.h"

#include <Arduino.h>
#include <WiFi.h>

#include "build_info.generated.h"
#include "ota/auto_update_config.h"
#include "ota/update_engine.h"
#include <Update.h>

namespace {
const char *const kFirmwarePath = "release/firmware";
const char *const kSpiffsPath = "release/spiffs";
const char *const kVersionFilePath = "/version.txt";

AutoUpdateStatus g_status = {
    String(), String(), String(), String(), String("idle"), String(), 0, false, false, false};

unsigned long g_nextCheckMillis = 0;

void setState(const char *value) {
  g_status.state = value;
}

void setError(const String &message) {
  g_status.lastError = message;
  setState("error");
  Serial.print("[autoupdate] ");
  Serial.println(message);
}

bool isNewerBuildId(const String &candidate, const String &current) {
  return candidate.length() > 0 && (current.length() == 0 || candidate > current);
}

void refreshCurrentBuildIds() {
  g_status.currentFirmwareBuildId = RENOVENT_BUILD_ID;
  g_status.currentSpiffsBuildId = readSpiffsBuildId(kVersionFilePath);
}

void scheduleNextCheck(unsigned long delayMs) {
  g_nextCheckMillis = millis() + delayMs;
}

void performUpdateCheck() {
  if (!autoUpdateConfig::kEnabled || autoUpdateConfig::kGitHubOwner[0] == '\0' ||
      autoUpdateConfig::kGitHubRepo[0] == '\0') {
    setState("disabled");
    return;
  }

  refreshCurrentBuildIds();
  g_status.latestFirmwareBuildId = String();
  g_status.latestSpiffsBuildId = String();
  g_status.firmwareUpdateAvailable = false;
  g_status.spiffsUpdateAvailable = false;
  g_status.lastError = String();
  g_status.lastCheckMillis = millis();

  setState("checking");
  RemoteArtifact firmwareArtifact;
  if (!fetchLatestArtifact(autoUpdateConfig::kGitHubOwner, autoUpdateConfig::kGitHubRepo,
                           autoUpdateConfig::kGitHubRef, kFirmwarePath, autoUpdateConfig::kUserAgent,
                           firmwareArtifact, setError)) {
    return;
  }

  g_status.latestFirmwareBuildId = firmwareArtifact.buildId;
  g_status.firmwareUpdateAvailable = isNewerBuildId(firmwareArtifact.buildId, g_status.currentFirmwareBuildId);
  if (g_status.firmwareUpdateAvailable) {
    Serial.print("[autoupdate] Applying firmware ");
    Serial.println(firmwareArtifact.buildId);
    setState("updating-firmware");
    if (applyRemoteArtifact(firmwareArtifact, U_FLASH, autoUpdateConfig::kUserAgent, setError)) {
      delay(500);
      ESP.restart();
    }
    return;
  }

  RemoteArtifact spiffsArtifact;
  if (!fetchLatestArtifact(autoUpdateConfig::kGitHubOwner, autoUpdateConfig::kGitHubRepo,
                           autoUpdateConfig::kGitHubRef, kSpiffsPath, autoUpdateConfig::kUserAgent,
                           spiffsArtifact, setError)) {
    return;
  }

  g_status.latestSpiffsBuildId = spiffsArtifact.buildId;
  g_status.spiffsUpdateAvailable = isNewerBuildId(spiffsArtifact.buildId, g_status.currentSpiffsBuildId);
  if (g_status.spiffsUpdateAvailable) {
    Serial.print("[autoupdate] Applying spiffs ");
    Serial.println(spiffsArtifact.buildId);
    setState("updating-spiffs");
    if (applyRemoteArtifact(spiffsArtifact, U_SPIFFS, autoUpdateConfig::kUserAgent, setError)) {
      delay(500);
      ESP.restart();
    }
    return;
  }

  setState("idle");
}
}  // namespace

void setupAutoUpdate() {
  refreshCurrentBuildIds();
  g_status.latestFirmwareBuildId = g_status.currentFirmwareBuildId;
  g_status.latestSpiffsBuildId = g_status.currentSpiffsBuildId;
  g_status.checkQueued = true;
  scheduleNextCheck(autoUpdateConfig::kInitialCheckDelayMs);
  if (!autoUpdateConfig::kEnabled) {
    setState("disabled");
  }
}

void autoUpdateLoop() {
  if (!autoUpdateConfig::kEnabled) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  const unsigned long now = millis();
  if (!g_status.checkQueued && static_cast<long>(now - g_nextCheckMillis) < 0) {
    return;
  }

  if (g_status.checkQueued && static_cast<long>(now - g_nextCheckMillis) < 0) {
    return;
  }

  g_status.checkQueued = false;
  performUpdateCheck();
  if (g_status.state != "error") {
    setState("idle");
  }
  scheduleNextCheck(autoUpdateConfig::kCheckIntervalMs);
}

void queueAutoUpdateCheck() {
  g_status.checkQueued = true;
  scheduleNextCheck(0);
}

const AutoUpdateStatus &getAutoUpdateStatus() {
  return g_status;
}

String getCurrentFirmwareBuildId() {
  return RENOVENT_BUILD_ID;
}

String getCurrentSpiffsBuildId() {
  return readSpiffsBuildId(kVersionFilePath);
}