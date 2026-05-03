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
String formatHttpFailure(const char *label, const String &url, int httpCode) {
  String message = label;
  message += ": HTTP ";
  message += httpCode;
  message += " url=";
  message += url;
  return message;
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
                  const String &url,
                  const char *acceptHeader,
                  const char *userAgent,
                  UpdateErrorReporter reportError) {
  client.setInsecure();
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  if (!http.begin(client, url)) {
    reportError(String("Failed to open URL: ") + url);
    return false;
  }
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
  if (!beginRequest(http, client, manifestUrlString, "application/json",
                    userAgent, reportError)) {
    return false;
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    reportError(formatHttpFailure("Manifest fetch failed", manifestUrlString, httpCode));
    http.end();
    return false;
  }

  JsonDocument doc;
  const DeserializationError jsonError = deserializeJson(doc, http.getString());
  http.end();
  if (jsonError) {
    reportError(String("Failed to parse manifest: url=") + manifestUrlString + " error=" + jsonError.c_str());
    return false;
  }

  if (!doc.is<JsonObject>()) {
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
  if (!beginRequest(http, client, artifact.downloadUrl, "application/octet-stream", userAgent,
                    reportError)) {
    return false;
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    reportError(formatHttpFailure("Download failed", artifact.downloadUrl, httpCode));
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  if (contentLength <= 0) {
    reportError(String("Missing content length for ") + artifact.buildId);
    http.end();
    return false;
  }

  if (!Update.begin(contentLength, updateCommand)) {
    reportError(String("Update.begin failed: ") + Update.errorString());
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  const size_t written = Update.writeStream(*stream);
  http.end();

  if (written != static_cast<size_t>(contentLength)) {
    Update.abort();
    reportError(String("Incomplete download for ") + artifact.buildId);
    return false;
  }

  if (!Update.end()) {
    reportError(String("Update.end failed: ") + Update.errorString());
    return false;
  }

  if (!Update.isFinished()) {
    reportError(String("Update did not finish for ") + artifact.buildId);
    return false;
  }

  return true;
}