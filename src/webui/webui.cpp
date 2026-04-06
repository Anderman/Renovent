#include "webui.h"

#include <SPIFFS.h>
#include <WiFi.h>

#include "../co2_sensor.h"
#include "../display_reader.h"
#include "../sensors_menu.h"
#include "../input_keys.h"
#include "../keypad.h"
#include "../key_writer.h"
#include "../ota/auto_update.h"
#include "../parameter_definitions.h"
#include "../reset_info.h"
#include "../setting_writer.h"
#include "../settings_menu.h"

WebServer server(80);

namespace {
bool g_spiffsOk = false;

const char *contentTypeForPath(const String &path) {
  if (path.endsWith(".html")) {
    return "text/html";
  }
  if (path.endsWith(".css")) {
    return "text/css";
  }
  if (path.endsWith(".js")) {
    return "application/javascript";
  }
  if (path.endsWith(".json")) {
    return "application/json";
  }
  if (path.endsWith(".svg")) {
    return "image/svg+xml";
  }
  if (path.endsWith(".png")) {
    return "image/png";
  }
  return "text/plain";
}

void sendCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Accept");
}

bool tryServeFromSpiffs(const String &uri) {
  if (!g_spiffsOk) {
    return false;
  }

  String path = uri;
  if (path == "/") {
    path = "/index.html";
  }

  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    if (!file) {
      return false;
    }
    server.streamFile(file, contentTypeForPath(path));
    file.close();
    return true;
  }

  return false;
}

void handleStatus() {
  const DisplaySnapshot snapshot = getDisplaySnapshot();
  const DisplayReaderStats displayReaderStats = getDisplayReaderStats();
  const String activeKeys = activeKeysToString(snapshot.activeKeys);
  const Co2SensorStatus co2Status = getCo2SensorStatus();
  const SensorsMenuStatus sensorsMenuStatus = getSensorsMenuStatus();
  const AutoUpdateStatus &autoUpdateStatus = getAutoUpdateStatus();
  const KeyPressLogSummary keyPressLogStatus = getKeyPressLogSummary();
  const ResetInfoStatus resetInfoStatus = getResetInfoStatus();
  const SettingWriterStatus settingWriterStatus = getSettingWriterStatus();
  const SettingsMenuStatus settingsMenuStatus = getSettingsMenuStatus();

  JsonDocument doc;
  doc["uptimeMs"] = millis();
  doc["ip"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  doc["rssi"] = WiFi.RSSI();
  doc["resetBootCount"] = resetInfoStatus.bootCount;
  doc["resetReason"] = resetInfoStatus.reason;
  doc["resetDetail"] = resetInfoStatus.detail;
  doc["resetRawReason"] = resetInfoStatus.rawReason;
  doc["resetCrashLikely"] = resetInfoStatus.crashLikely;
  doc["displayText"] = snapshot.text;
  doc["displayCompleteFrameCount"] = displayReaderStats.completeFrameCount;
  doc["displayMissedSelectFrameCount"] = displayReaderStats.missedSelectFrameCount;
  doc["displayPublishedFrameCount"] = displayReaderStats.publishedFrameCount;
  doc["displayMissedSelectPercent"] = displayReaderStats.missedSelectPercent;
  doc["activeKeys"] = activeKeys;
  doc["loggedActiveKeys"] = keyPressLogStatus.activeKeys;
  doc["loggedActiveRelativeMs"] = keyPressLogStatus.activeRelativeMs;
  doc["loggedReleaseForMs"] = keyPressLogStatus.releaseForMs;
  doc["loggedDisplayText"] = keyPressLogStatus.lastDisplayText;
  doc["co2Connected"] = co2Status.connected;
  doc["co2Measuring"] = co2Status.measuring;
  doc["co2Valid"] = co2Status.dataValid;
  doc["co2Ppm"] = co2Status.dataValid ? co2Status.co2Ppm : 0;
  doc["co2TemperatureC"] = co2Status.dataValid ? co2Status.temperatureC : 0.0f;
  doc["co2HumidityPct"] = co2Status.dataValid ? co2Status.humidityPct : 0.0f;
  doc["co2LastSampleMs"] = co2Status.lastSampleMs;
  doc["co2Error"] = co2Status.error;
  doc["sensorsMenuRunning"] = sensorsMenuStatus.running;
  doc["sensorsMenuDone"] = sensorsMenuStatus.done;
  doc["sensorsMenuPhase"] = sensorsMenuStatus.phase;
  doc["sensorsMenuStep"] = sensorsMenuStatus.currentStep;
  doc["sensorsMenuDisplay"] = sensorsMenuStatus.lastDisplayText;
  doc["sensorsMenuLastCompletedMs"] = sensorsMenuStatus.lastCompletedMs;
  doc["sensorsMenuAutoScanEnabled"] = sensorsMenuAutoScanEnabled();
  doc["firmwareBuildId"] = autoUpdateStatus.currentFirmwareBuildId;
  doc["spiffsBuildId"] = autoUpdateStatus.currentSpiffsBuildId;
  doc["autoUpdateState"] = autoUpdateStatus.state;
  doc["autoUpdateError"] = autoUpdateStatus.lastError;
  doc["autoUpdateLastCheckMs"] = autoUpdateStatus.lastCheckMillis;
  doc["firmwareUpdateAvailable"] = autoUpdateStatus.firmwareUpdateAvailable;
  doc["spiffsUpdateAvailable"] = autoUpdateStatus.spiffsUpdateAvailable;
  doc["settingsMenuRunning"] = settingsMenuStatus.running;
  doc["settingsMenuCount"] = settingsMenuStatus.count;
  doc["settingsMenuPhase"] = settingsMenuStatus.phase;
  doc["settingsMenuLastCompletedMs"] = settingsMenuStatus.lastCompletedMs;
  doc["settingWriterRunning"] = settingWriterStatus.running;
  doc["settingWriterKey"] = settingWriterStatus.key;
  doc["settingWriterCurrentValue"] = settingWriterStatus.currentValue;
  doc["settingWriterTargetValue"] = settingWriterStatus.targetValue;
  doc["settingWriterPhase"] = settingWriterStatus.phase;
  doc["settingWriterDisplay"] = settingWriterStatus.lastDisplayText;
  doc["settingWriterLastCompletedMs"] = settingWriterStatus.lastCompletedMs;

  JsonArray keyPressLog = doc["keyPressLog"].to<JsonArray>();
  for (uint16_t index = 0; index < keyPressLogStatus.count; ++index) {
    KeyPressLogEntry logEntry{};
    if (!getKeyPressLogEntryNewestFirst(index, logEntry)) {
      continue;
    }
    JsonObject entry = keyPressLog.add<JsonObject>();
    entry["event"] = logEntry.event;
    entry["keys"] = logEntry.keys;
    entry["mask"] = logEntry.mask;
    entry["display"] = logEntry.display;
    entry["relativeMs"] = logEntry.relativeMs;
    entry["idleBeforeMs"] = logEntry.idleBeforeMs;
    entry["releaseForMs"] = logEntry.releaseForMs;
  }

  JsonArray settingsMenuEntries = doc["settingsMenuEntries"].to<JsonArray>();
  for (uint8_t index = 0; index < settingsMenuStatus.count; ++index) {
    SettingsMenuValue settingsMenuValue{};
    if (!getSettingsMenuValue(index, settingsMenuValue)) {
      continue;
    }
    JsonObject entry = settingsMenuEntries.add<JsonObject>();
    entry["key"] = settingsMenuValue.key;
    entry["available"] = settingsMenuValue.available;
    entry["rawValue"] = settingsMenuValue.available ? settingsMenuValue.rawValue : "";
    entry["hasValue"] = settingsMenuValue.hasValue;
    entry["value"] = settingsMenuValue.value;
  }

  String output;
  serializeJson(doc, output);
  sendCorsHeaders();
  server.send(200, "application/json", output);
}

void handleSensorsMenuGet() {
  const SensorsMenuStatus sensorsMenuStatus = getSensorsMenuStatus();

  JsonDocument doc;
  doc["running"] = sensorsMenuStatus.running;
  doc["done"] = sensorsMenuStatus.done;
  doc["autoScanEnabled"] = sensorsMenuAutoScanEnabled();
  doc["phase"] = sensorsMenuStatus.phase;
  doc["currentStep"] = sensorsMenuStatus.currentStep;
  doc["lastCompletedMs"] = sensorsMenuStatus.lastCompletedMs;
  doc["lastDisplayText"] = sensorsMenuStatus.lastDisplayText;

  JsonArray entries = doc["entries"].to<JsonArray>();
  for (uint8_t index = 0; index < 13; ++index) {
    const SensorsMenuDefinition definition = getSensorsMenuDefinition(index + 1U);
    JsonObject entry = entries.add<JsonObject>();
    entry["step"] = index + 1;
    entry["example"] = definition.example;
    entry["description"] = definition.description;
    entry["remark"] = definition.remark;
    entry["available"] = sensorsMenuStatus.entries[index].available;
    entry["rawValue"] = sensorsMenuStatus.entries[index].available ? sensorsMenuStatus.entries[index].rawValue : "";
    entry["detail"] = sensorsMenuStatus.entries[index].available ? sensorsMenuStatus.entries[index].detail : "";
    entry["hasValue"] = sensorsMenuStatus.entries[index].hasValue;
    entry["value"] = sensorsMenuStatus.entries[index].value;
    entry["hasAuxValue"] = sensorsMenuStatus.entries[index].hasAuxValue;
    entry["auxValue"] = sensorsMenuStatus.entries[index].auxValue;
  }

  String output;
  serializeJson(doc, output);
  sendCorsHeaders();
  server.send(200, "application/json", output);
}

void handleSettingsMenuRead() {
  const bool scheduled = requestSettingsMenuRead();

  JsonDocument doc;
  doc["scheduled"] = scheduled;

  String output;
  serializeJson(doc, output);
  sendCorsHeaders();
  server.send(scheduled ? 202 : 409, "application/json", output);
}

void handleParameterDefinitions() {
  JsonDocument doc;
  JsonArray entries = doc["entries"].to<JsonArray>();

  for (size_t index = 0; index < getParameterDefinitionCount(); ++index) {
    const ParameterDefinition *definition = getParameterDefinitionAt(index);
    if (definition == nullptr) {
      continue;
    }

    JsonObject entry = entries.add<JsonObject>();
    entry["key"] = definition->key;
    entry["title"] = definition->title;
    entry["description"] = definition->description;
    entry["range"] = definition->range;
    entry["defaultValue"] = definition->defaultValue;
  }

  String output;
  serializeJson(doc, output);
  sendCorsHeaders();
  server.send(200, "application/json", output);
}

void handleSetValue() {
  JsonDocument doc;
  if (!parseJsonBody(doc)) {
    return;
  }

  const char *requestedKey = doc["key"] | "";
  const int32_t requestedValue = doc["value"] | 0;
  const bool scheduled = requestSettingWrite(requestedKey, requestedValue);

  JsonDocument response;
  response["scheduled"] = scheduled;
  response["key"] = scheduled ? requestedKey : "";
  response["value"] = scheduled ? requestedValue : 0;

  String output;
  serializeJson(response, output);
  sendCorsHeaders();
  server.send(scheduled ? 202 : 409, "application/json", output);
}

void handleSensorsMenuStart() {
  const bool allowed = !sensorsMenuIsBusy() && !settingsMenuIsBusy() && !settingWriterIsBusy();
  if (allowed) {
    startSensorsMenuScan();
  }

  JsonDocument doc;
  doc["scheduled"] = allowed;

  String output;
  serializeJson(doc, output);
  sendCorsHeaders();
  server.send(allowed ? 202 : 409, "application/json", output);
}

void handleKeyPress() {
  JsonDocument doc;
  if (!parseJsonBody(doc)) {
    return;
  }

  const int requestedMask = doc["mask"] | 0;
  const uint32_t durationMs = doc["durationMs"] | 0;
  const bool allowed = requestedMask >= 0 && requestedMask <= 15;
  if (allowed) {
    if (durationMs > 0) {
      pulseKeys(static_cast<uint8_t>(requestedMask), durationMs);
    } else {
      pressKeys(static_cast<uint8_t>(requestedMask));
    }
  }

  JsonDocument response;
  response["accepted"] = allowed;
  response["mask"] = allowed ? requestedMask : 0;
  response["durationMs"] = allowed ? durationMs : 0;

  String output;
  serializeJson(response, output);
  sendCorsHeaders();
  server.send(allowed ? 202 : 409, "application/json", output);
}

void handleNotFound() {
  if (tryServeFromSpiffs(server.uri())) {
    return;
  }

  sendCorsHeaders();
  server.send(404, "text/plain", "Not found");
}
}

bool parseJsonBody(JsonDocument &doc) {
  const String body = server.arg("plain");
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "text/plain", "Invalid JSON");
    return false;
  }
  return true;
}

void setupWebUi() {
  g_spiffsOk = SPIFFS.begin(false);

  server.on("/api/parameter-definitions", HTTP_GET, handleParameterDefinitions);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/sensors-menu", HTTP_GET, handleSensorsMenuGet);
  server.on("/api/sensors-menu/start", HTTP_POST, handleSensorsMenuStart);
  server.on("/api/set-value", HTTP_POST, handleSetValue);
  server.on("/api/keys/press", HTTP_POST, handleKeyPress);
  server.on("/api/settings-menu/read", HTTP_POST, handleSettingsMenuRead);
  server.onNotFound(handleNotFound);
  server.begin();
}

void webUiLoop() {
  server.handleClient();
}