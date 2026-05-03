#include "ota/update_engine.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>

namespace {
UpdateHttpCall g_lastHttpCall;
constexpr uint8_t kHttpHistoryCapacity = 4;
String g_httpHistory[kHttpHistoryCapacity];
uint8_t g_httpHistoryCount = 0;
uint8_t g_httpHistoryNextIndex = 0;

String formatHttpFailure(const char *label, const String &url, int httpCode) {
  String message = label;
  message += ": HTTP ";
  message += httpCode;
  message += " url=";
  message += url;
  return message;
}

void setLastHttpCall(const String &operation,
                     const String &url,
                     int httpCode,
                     const String &detail) {
  g_lastHttpCall.operation = operation;
  g_lastHttpCall.url = url;
  g_lastHttpCall.httpCode = httpCode;
  g_lastHttpCall.detail = detail;

  String entry = operation;
  if (httpCode > 0) {
    entry += " HTTP ";
    entry += httpCode;
  }
  if (detail.length() > 0) {
    entry += " ";
    entry += detail;
  }
  if (url.length() > 0) {
    entry += " url=";
    entry += url;
  }

  g_httpHistory[g_httpHistoryNextIndex] = entry;
  g_httpHistoryNextIndex = static_cast<uint8_t>((g_httpHistoryNextIndex + 1U) % kHttpHistoryCapacity);
  if (g_httpHistoryCount < kHttpHistoryCapacity) {
    ++g_httpHistoryCount;
  }
}

bool isBuildId(String value) {
  if (!value.endsWith(".bin")) {
    return false;
  }

  value.remove(value.length() - 4);
  if (value.length() != 16 || value.charAt(8) != 'T' || value.charAt(15) != 'Z') {
    return false;
  }

  for (int index = 0; index < value.length(); ++index) {
    if (index == 8 || index == 15) {
      continue;
    }

    if (!isDigit(value.charAt(index))) {
      return false;
    }
  }

  return true;
}

String extractBuildId(const String &fileName) {
  if (!isBuildId(fileName)) {
    return String();
  }

  return fileName.substring(0, fileName.length() - 4);
}

bool isNewerBuildId(const String &candidate, const String &current) {
  return candidate.length() > 0 && (current.length() == 0 || candidate > current);
}

bool readArtifactFromManifest(JsonVariantConst value,
                              const char *name,
                              RemoteArtifact &artifact,
                              const String &manifestUrl,
                              UpdateErrorReporter reportError) {
  if (!value.is<JsonObjectConst>()) {
    reportError(String("Invalid manifest entry: ") + name + " url=" + manifestUrl);
    return false;
  }

  const char *buildId = value["buildId"];
  const char *downloadUrl = value["downloadUrl"];
  if (buildId == nullptr || downloadUrl == nullptr) {
    reportError(String("Incomplete manifest entry: ") + name + " url=" + manifestUrl);
    return false;
  }

  artifact.buildId = buildId;
  artifact.downloadUrl = downloadUrl;
  if (!isBuildId(String(artifact.buildId) + ".bin")) {
    reportError(String("Invalid buildId in manifest: ") + name + " value=" + artifact.buildId + " url=" + manifestUrl);
    return false;
  }

  return true;
}

bool beginRequest(HTTPClient &http,
                  WiFiClientSecure &client,
                  const char *operation,
                  const String &url,
                  const char *acceptHeader,
                  const char *userAgent,
                  UpdateErrorReporter reportError) {
  client.setInsecure();
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  if (!http.begin(client, url)) {
    setLastHttpCall(operation, url, 0, "http.begin failed");
    reportError(String("Failed to open URL: ") + url);
    return false;
  }
  setLastHttpCall(operation, url, 0, "request opened");
  http.addHeader("User-Agent", userAgent);
  http.addHeader("Accept", acceptHeader);
  return true;
}
}  // namespace

bool fetchLatestArtifactsManifest(const char *manifestUrl,
                                  const char *userAgent,
                                  RemoteArtifact &firmwareArtifact,
                                  RemoteArtifact &spiffsArtifact,
                                  UpdateErrorReporter reportError) {
  const String manifestUrlString = manifestUrl;
  WiFiClientSecure client;
  HTTPClient http;
  if (!beginRequest(http, client, "manifest", manifestUrlString, "application/json",
                    userAgent, reportError)) {
    return false;
  }

  const int httpCode = http.GET();
  setLastHttpCall("manifest", manifestUrlString, httpCode,
                  httpCode == HTTP_CODE_OK ? "manifest response ok" : "manifest fetch failed");
  if (httpCode != HTTP_CODE_OK) {
    reportError(formatHttpFailure("Manifest fetch failed", manifestUrlString, httpCode));
    http.end();
    return false;
  }

  JsonDocument doc;
  const DeserializationError jsonError = deserializeJson(doc, http.getString());
  http.end();
  if (jsonError) {
    setLastHttpCall("manifest", manifestUrlString, HTTP_CODE_OK,
                    String("json parse failed: ") + jsonError.c_str());
    reportError(String("Failed to parse manifest: url=") + manifestUrlString + " error=" + jsonError.c_str());
    return false;
  }

  if (!doc.is<JsonObject>()) {
    setLastHttpCall("manifest", manifestUrlString, HTTP_CODE_OK, "unexpected manifest payload");
    reportError(String("Unexpected manifest payload: url=") + manifestUrlString);
    return false;
  }

  return readArtifactFromManifest(doc["firmware"], "firmware", firmwareArtifact, manifestUrlString, reportError) &&
         readArtifactFromManifest(doc["spiffs"], "spiffs", spiffsArtifact, manifestUrlString, reportError);
}

String readSpiffsBuildId(const char *versionFilePath) {
  if (!SPIFFS.begin(false)) {
    return String();
  }

  if (!SPIFFS.exists(versionFilePath)) {
    return String();
  }

  File file = SPIFFS.open(versionFilePath, "r");
  if (!file) {
    return String();
  }

  String buildId = file.readString();
  file.close();
  buildId.trim();
  return buildId;
}

bool applyRemoteArtifact(const RemoteArtifact &artifact,
                        int updateCommand,
                        const char *userAgent,
                        UpdateErrorReporter reportError) {
  WiFiClientSecure client;
  HTTPClient http;
  if (!beginRequest(http, client, "artifact", artifact.downloadUrl, "application/octet-stream", userAgent,
                    reportError)) {
    return false;
  }

  const int httpCode = http.GET();
  setLastHttpCall(String("artifact:") + artifact.buildId, artifact.downloadUrl, httpCode,
                  httpCode == HTTP_CODE_OK ? "download response ok" : "artifact download failed");
  if (httpCode != HTTP_CODE_OK) {
    reportError(formatHttpFailure("Download failed", artifact.downloadUrl, httpCode));
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  if (contentLength <= 0) {
    setLastHttpCall(String("artifact:") + artifact.buildId, artifact.downloadUrl, httpCode,
                    "missing content length");
    reportError(String("Missing content length for ") + artifact.buildId);
    http.end();
    return false;
  }

  if (!Update.begin(contentLength, updateCommand)) {
    setLastHttpCall(String("artifact:") + artifact.buildId, artifact.downloadUrl, httpCode,
                    String("Update.begin failed: ") + Update.errorString());
    reportError(String("Update.begin failed: ") + Update.errorString());
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  const size_t written = Update.writeStream(*stream);
  http.end();

  if (written != static_cast<size_t>(contentLength)) {
    Update.abort();
    setLastHttpCall(String("artifact:") + artifact.buildId, artifact.downloadUrl, httpCode,
                    String("incomplete download ") + written + "/" + contentLength);
    reportError(String("Incomplete download for ") + artifact.buildId);
    return false;
  }

  if (!Update.end()) {
    setLastHttpCall(String("artifact:") + artifact.buildId, artifact.downloadUrl, httpCode,
                    String("Update.end failed: ") + Update.errorString());
    reportError(String("Update.end failed: ") + Update.errorString());
    return false;
  }

  if (!Update.isFinished()) {
    setLastHttpCall(String("artifact:") + artifact.buildId, artifact.downloadUrl, httpCode,
                    "update not finished");
    reportError(String("Update did not finish for ") + artifact.buildId);
    return false;
  }

  setLastHttpCall(String("artifact:") + artifact.buildId, artifact.downloadUrl, httpCode,
                  String("downloaded ") + contentLength + " bytes");

  return true;
}

const UpdateHttpCall &getLastUpdateHttpCall() {
  return g_lastHttpCall;
}

String getRecentUpdateHttpCallsText() {
  String text;
  for (uint8_t index = 0; index < g_httpHistoryCount; ++index) {
    const uint8_t historyIndex = static_cast<uint8_t>((g_httpHistoryNextIndex + kHttpHistoryCapacity - 1U - index) % kHttpHistoryCapacity);
    if (g_httpHistory[historyIndex].length() == 0) {
      continue;
    }

    if (text.length() > 0) {
      text += " || ";
    }
    text += g_httpHistory[historyIndex];
  }

  return text;
}