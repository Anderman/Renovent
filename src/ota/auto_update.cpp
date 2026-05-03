#include "ota/auto_update.h"

#include <Arduino.h>
#include <WiFi.h>

#include "build_info.generated.h"
#include "ota/auto_update_config.h"
#include "ota/update_engine.h"
#include <Update.h>

namespace {
const char *const kVersionFilePath = "/version.txt";

AutoUpdateStatus g_status = {
  String(), String(), String(), String(), String("idle"), String(), 0, 0, 0, 0, 0,
  String("Nog geen check uitgevoerd"), 0, {}, false, false, false};

unsigned long g_nextCheckMillis = 0;

void appendLog(const String &message) {
  Serial.print("[autoupdate] ");
  Serial.println(message);

  if (g_status.logCount < kAutoUpdateLogCapacity) {
    g_status.logEntries[g_status.logCount++] = message;
    return;
  }

  for (uint8_t index = 1; index < kAutoUpdateLogCapacity; ++index) {
    g_status.logEntries[index - 1] = g_status.logEntries[index];
  }
  g_status.logEntries[kAutoUpdateLogCapacity - 1] = message;
}

void setState(const char *value) {
  g_status.state = value;
}

void setError(const String &message) {
  g_status.lastError = message;
  g_status.lastCheckResult = message;
  setState("error");
  appendLog(String("Fout: ") + message);
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
  g_status.nextCheckMillis = g_nextCheckMillis;
}

void performUpdateCheck() {
  if (!autoUpdateConfig::kEnabled || autoUpdateConfig::kGitHubOwner[0] == '\0' ||
      autoUpdateConfig::kGitHubRepo[0] == '\0') {
    g_status.lastCheckResult = "Auto-update is uitgeschakeld";
    setState("disabled");
    return;
  }

  const unsigned long startedMs = millis();
  refreshCurrentBuildIds();
  g_status.latestFirmwareBuildId = String();
  g_status.latestSpiffsBuildId = String();
  g_status.firmwareUpdateAvailable = false;
  g_status.spiffsUpdateAvailable = false;
  g_status.lastError = String();
  g_status.lastCheckMillis = startedMs;
  g_status.lastCheckDurationMs = 0;
  g_status.checkCount += 1;
  g_status.lastCheckResult = "Controleren op update";

  setState("checking");
  appendLog(String("Check #") + g_status.checkCount + " gestart");

  RemoteArtifact firmwareArtifact;
  RemoteArtifact spiffsArtifact;
  if (!fetchLatestArtifactsManifest(autoUpdateConfig::kGitHubOwner, autoUpdateConfig::kGitHubRepo,
                                    autoUpdateConfig::kGitHubRef, autoUpdateConfig::kUserAgent,
                                    firmwareArtifact, spiffsArtifact, setError)) {
    g_status.lastCheckDurationMs = millis() - startedMs;
    return;
  }

  g_status.latestFirmwareBuildId = firmwareArtifact.buildId;
  g_status.latestSpiffsBuildId = spiffsArtifact.buildId;
  g_status.firmwareUpdateAvailable = isNewerBuildId(firmwareArtifact.buildId, g_status.currentFirmwareBuildId);
  g_status.spiffsUpdateAvailable = isNewerBuildId(spiffsArtifact.buildId, g_status.currentSpiffsBuildId);
  if (g_status.firmwareUpdateAvailable) {
    g_status.successfulCheckCount += 1;
    g_status.lastCheckDurationMs = millis() - startedMs;
    g_status.lastCheckResult = String("Nieuwe firmware ") + firmwareArtifact.buildId;
    appendLog(String("Nieuwe firmware gevonden: ") + firmwareArtifact.buildId);
    setState("updating-firmware");
    if (applyRemoteArtifact(firmwareArtifact, U_FLASH, autoUpdateConfig::kUserAgent, setError)) {
      appendLog(String("Firmware update toegepast: ") + firmwareArtifact.buildId);
      delay(500);
      ESP.restart();
    }
    return;
  }

  if (g_status.spiffsUpdateAvailable) {
    g_status.successfulCheckCount += 1;
    g_status.lastCheckDurationMs = millis() - startedMs;
    g_status.lastCheckResult = String("Nieuwe SPIFFS ") + spiffsArtifact.buildId;
    appendLog(String("Nieuwe SPIFFS gevonden: ") + spiffsArtifact.buildId);
    setState("updating-spiffs");
    if (applyRemoteArtifact(spiffsArtifact, U_SPIFFS, autoUpdateConfig::kUserAgent, setError)) {
      appendLog(String("SPIFFS update toegepast: ") + spiffsArtifact.buildId);
      delay(500);
      ESP.restart();
    }
    return;
  }

  g_status.successfulCheckCount += 1;
  g_status.lastCheckDurationMs = millis() - startedMs;
  g_status.lastCheckResult = "Geen update beschikbaar";
  appendLog("Geen update beschikbaar");
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
    g_status.lastCheckResult = "Auto-update is uitgeschakeld";
    setState("disabled");
    appendLog("Auto-update is uitgeschakeld");
  } else {
    appendLog(String("Auto-update actief, eerste check over ") + (autoUpdateConfig::kInitialCheckDelayMs / 1000UL) + "s");
  }
}

void autoUpdateLoop() {
  if (!autoUpdateConfig::kEnabled) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    g_status.lastCheckResult = "Wacht op WiFi";
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
  appendLog(String("Volgende check over ") + (autoUpdateConfig::kCheckIntervalMs / 1000UL) + "s");
}

void queueAutoUpdateCheck() {
  g_status.checkQueued = true;
  g_status.lastCheckResult = "Handmatige check ingepland";
  scheduleNextCheck(0);
  appendLog("Handmatige check ingepland");
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