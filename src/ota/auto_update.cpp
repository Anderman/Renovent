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
  String(), String(), String("idle"), 0, 0, String("Nog geen check uitgevoerd")};

unsigned long g_nextCheckMillis = 0;

void logMessage(const String &message) {
  Serial.print("[autoupdate] ");
  Serial.println(message);
}

void setState(const char *value) {
  g_status.state = value;
}

void setError(const String &message) {
  g_status.lastCheckResult = message;
  setState("error");
  logMessage(String("Fout: ") + message);
}

bool isNewerBuildId(const String &candidate, const String &current) {
  return candidate.length() > 0 && (current.length() == 0 || candidate > current);
}

bool autoUpdateEnabled() {
  return autoUpdateConfig::kEnabled && autoUpdateConfig::kManifestUrl[0] != '\0';
}

void refreshCurrentBuildIds() {
  g_status.currentFirmwareBuildId = RENOVENT_BUILD_ID;
  g_status.currentSpiffsBuildId = readSpiffsBuildId(kVersionFilePath);
}

void scheduleNextCheck(unsigned long delayMs) {
  g_nextCheckMillis = millis() + delayMs;
  g_status.nextCheckMillis = g_nextCheckMillis;
}

void beginUpdateCheck(unsigned long startedMs) {
  refreshCurrentBuildIds();
  g_status.lastCheckMillis = startedMs;
  g_status.lastCheckResult = "Controleren op update";
  setState("checking");
  logMessage("Check gestart");
}

void completeUpdateCheck(const String &result) {
  g_status.lastCheckResult = result;
  logMessage(result);
  setState("idle");
}

bool applyAvailableArtifact(const RemoteArtifact &artifact,
                           int updateCommand,
                           const char *updatingState,
                           const char *foundPrefix,
                           const char *appliedPrefix) {
  g_status.lastCheckResult = String(foundPrefix) + artifact.buildId;
  logMessage(g_status.lastCheckResult);
  setState(updatingState);

  if (applyRemoteArtifact(artifact, updateCommand, autoUpdateConfig::kUserAgent, setError)) {
    logMessage(String(appliedPrefix) + artifact.buildId);
    delay(500);
    ESP.restart();
  }

  return true;
}

void performUpdateCheck() {
  if (!autoUpdateEnabled()) {
    g_status.lastCheckResult = "Auto-update is uitgeschakeld";
    setState("disabled");
    return;
  }

  const unsigned long startedMs = millis();
  beginUpdateCheck(startedMs);

  RemoteArtifact firmwareArtifact;
  RemoteArtifact spiffsArtifact;
  if (!fetchLatestArtifactsManifest(autoUpdateConfig::kManifestUrl, autoUpdateConfig::kUserAgent,
                                    firmwareArtifact, spiffsArtifact, setError)) {
    return;
  }

  if (isNewerBuildId(firmwareArtifact.buildId, g_status.currentFirmwareBuildId)) {
    applyAvailableArtifact(firmwareArtifact, U_FLASH, "updating-firmware", "Nieuwe firmware ", "Firmware update toegepast: ");
    return;
  }

  if (isNewerBuildId(spiffsArtifact.buildId, g_status.currentSpiffsBuildId)) {
    applyAvailableArtifact(spiffsArtifact, U_SPIFFS, "updating-spiffs", "Nieuwe SPIFFS ", "SPIFFS update toegepast: ");
    return;
  }

  completeUpdateCheck("Geen update beschikbaar");
}
}  // namespace

void setupAutoUpdate() {
  refreshCurrentBuildIds();
  scheduleNextCheck(autoUpdateConfig::kInitialCheckDelayMs);
  if (!autoUpdateEnabled()) {
    g_status.lastCheckResult = "Auto-update is uitgeschakeld";
    setState("disabled");
    logMessage("Auto-update is uitgeschakeld");
  } else {
    logMessage(String("Auto-update actief, eerste check over ") + (autoUpdateConfig::kInitialCheckDelayMs / 1000UL) + "s");
  }
}

void autoUpdateLoop() {
  if (!autoUpdateEnabled()) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    g_status.lastCheckResult = "Wacht op WiFi";
    return;
  }

  const unsigned long now = millis();
  if (static_cast<long>(now - g_nextCheckMillis) < 0) {
    return;
  }

  performUpdateCheck();
  if (g_status.state != "error") {
    setState("idle");
  }
  scheduleNextCheck(autoUpdateConfig::kCheckIntervalMs);
  logMessage(String("Volgende check over ") + (autoUpdateConfig::kCheckIntervalMs / 1000UL) + "s");
}

void queueAutoUpdateCheck() {
  g_status.lastCheckResult = "Handmatige check ingepland";
  scheduleNextCheck(0);
  logMessage("Handmatige check ingepland");
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