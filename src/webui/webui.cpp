#include "webui.h"

#include <SPIFFS.h>
#include <WiFi.h>

#include "../co2_sensor.h"
#include "../display_reader.h"
#include "../sensors_menu.h"
#include "../input_keys.h"
#include "../keypad.h"
#include "../key_writer.h"
#include "../mqtt_config.h"
#include "../mqtt_discovery.h"
#include "../ota/auto_update.h"
#include "../parameter_definitions.h"
#include "../reset_info.h"
#include "../setting_writer.h"
#include "../settings_menu.h"

WebServer server(80);

namespace {
bool g_spiffsOk = false;
constexpr uint16_t kMaxKeyLogApiEntries = 60;

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
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
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
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.streamFile(file, contentTypeForPath(path));
    file.close();
    return true;
  }

  return false;
}

void handleStatus() {
  const DisplaySnapshot snapshot = getDisplaySnapshot();
  const String activeKeys = activeKeysToString(snapshot.activeKeys);
  const Co2SensorStatus co2Status = getCo2SensorStatus();
  const SensorsMenuStatus sensorsMenuStatus = getSensorsMenuStatus();
  const AutoUpdateStatus &autoUpdateStatus = getAutoUpdateStatus();
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
  doc["coreDumpPresent"] = resetInfoStatus.coreDumpPresent;
  doc["coreDumpSize"] = resetInfoStatus.coreDumpSize;
  doc["coreDumpState"] = resetInfoStatus.coreDumpState;
  doc["coreDumpReason"] = resetInfoStatus.coreDumpReason;
  doc["coreDumpBacktrace"] = resetInfoStatus.coreDumpBacktrace;
  doc["coreDumpBacktraceCorrupted"] = resetInfoStatus.coreDumpBacktraceCorrupted;
  doc["displayText"] = snapshot.text;
  doc["activeKeys"] = activeKeys;
  doc["co2Connected"] = co2Status.connected;
  doc["co2Measuring"] = co2Status.measuring;
  doc["co2Valid"] = co2Status.dataValid;
  doc["co2Ppm"] = co2Status.dataValid ? co2Status.co2Ppm : 0;
  doc["co2TemperatureC"] = co2Status.dataValid ? co2Status.temperatureC : 0.0f;
  doc["co2HumidityPct"] = co2Status.dataValid ? co2Status.humidityPct : 0.0f;
  doc["co2AbsoluteHumidityGm3"] = co2Status.dataValid ? co2Status.absoluteHumidityGm3 : 0.0f;
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
  doc["autoUpdateLastCheckMs"] = autoUpdateStatus.lastCheckMillis;
  doc["autoUpdateNextCheckMs"] = autoUpdateStatus.nextCheckMillis;
  doc["autoUpdateLastResult"] = autoUpdateStatus.lastCheckResult;
  doc["autoUpdateLastError"] = autoUpdateStatus.lastError;
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

void handleKeyPressLogGet() {
  const uint16_t keyPressLogCount = getKeyPressLogCount();
  const uint16_t returnedKeyLogEntries = keyPressLogCount > kMaxKeyLogApiEntries
      ? kMaxKeyLogApiEntries
      : keyPressLogCount;

  JsonDocument doc;
  doc["totalCount"] = keyPressLogCount;
  doc["returnedCount"] = returnedKeyLogEntries;

  JsonArray keyPressLog = doc["entries"].to<JsonArray>();
  for (uint16_t index = 0; index < returnedKeyLogEntries; ++index) {
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

  JsonArray values = doc["values"].to<JsonArray>();
  for (uint8_t index = 0; index < 14; ++index) {
    const SensorsMenuValueDefinition definition = getSensorsMenuValueDefinition(index + 1U);
    JsonObject value = values.add<JsonObject>();
    value["index"] = index + 1;
    value["key"] = definition.key;
    value["description"] = definition.description;
    value["unit"] = definition.unit;
    value["remark"] = definition.remark;
    value["available"] = sensorsMenuStatus.values[index].available;
    value["hasValue"] = sensorsMenuStatus.values[index].hasValue;
    value["value"] = sensorsMenuStatus.values[index].value;
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

void handleMqttConfigGet() {
  const MqttConfig &mqttConfig = getMqttConfig();

  JsonDocument doc;
  doc["mqttNodeId"] = mqttConfig.mqttNodeId;
  doc["mqttHost"] = mqttConfig.mqttHost;
  doc["mqttPort"] = mqttConfig.mqttPort;
  doc["mqttUser"] = mqttConfig.mqttUser;

  String output;
  serializeJson(doc, output);
  sendCorsHeaders();
  server.send(200, "application/json", output);
}

void handleMqttConfigPost() {
  JsonDocument doc;
  if (!parseJsonBody(doc)) {
    return;
  }

  const MqttConfig &currentConfig = getMqttConfig();
  MqttConfig nextConfig = currentConfig;

  nextConfig.mqttNodeId = String(doc["mqttNodeId"] | currentConfig.mqttNodeId.c_str());
  nextConfig.mqttHost = String(doc["mqttHost"] | currentConfig.mqttHost.c_str());
  nextConfig.mqttPort = static_cast<uint16_t>(doc["mqttPort"] | currentConfig.mqttPort);
  nextConfig.mqttUser = String(doc["mqttUser"] | currentConfig.mqttUser.c_str());

  const bool hasPasswordField = !doc["mqttPassword"].isNull();
  const String requestedPassword = String(doc["mqttPassword"] | "");
  if (hasPasswordField && requestedPassword.length() > 0) {
    nextConfig.mqttPassword = requestedPassword;
  }

  const bool saved = updateMqttConfig(nextConfig);
  const MqttConfig &savedConfig = getMqttConfig();
  if (saved) {
    mqttDiscoveryConfigChanged();
  }

  JsonDocument response;
  response["saved"] = saved;
  response["mqttNodeId"] = savedConfig.mqttNodeId;
  response["mqttHost"] = savedConfig.mqttHost;
  response["mqttPort"] = savedConfig.mqttPort;
  response["mqttUser"] = savedConfig.mqttUser;

  String output;
  serializeJson(response, output);
  sendCorsHeaders();
  server.send(saved ? 200 : 500, "application/json", output);
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
  server.on("/api/key-press-log", HTTP_GET, handleKeyPressLogGet);
  server.on("/api/mqtt/config", HTTP_GET, handleMqttConfigGet);
  server.on("/api/mqtt/config", HTTP_POST, handleMqttConfigPost);
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