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
const char *const kGitHubApiHost = "https://api.github.com/repos/";

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

String getContentsUrl(const char *owner, const char *repo, const char *ref, const char *path) {
  String url = kGitHubApiHost;
  url += owner;
  url += "/";
  url += repo;
  url += "/contents/";
  url += path;
  url += "?ref=";
  url += ref;
  return url;
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

bool fetchLatestArtifact(const char *owner,
                        const char *repo,
                        const char *ref,
                        const char *path,
                        const char *userAgent,
                        RemoteArtifact &artifact,
                        UpdateErrorReporter reportError) {
  WiFiClientSecure client;
  HTTPClient http;
  if (!beginRequest(http, client, getContentsUrl(owner, repo, ref, path), "application/vnd.github+json",
                    userAgent, reportError)) {
    return false;
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    reportError(String("GitHub listing failed for ") + path + ": HTTP " + httpCode);
    http.end();
    return false;
  }

  JsonDocument doc;
  const DeserializationError jsonError = deserializeJson(doc, http.getString());
  http.end();
  if (jsonError) {
    reportError(String("Failed to parse GitHub listing for ") + path + ": " + jsonError.c_str());
    return false;
  }

  if (!doc.is<JsonArray>()) {
    reportError(String("Unexpected GitHub listing for ") + path);
    return false;
  }

  for (JsonObject item : doc.as<JsonArray>()) {
    const char *name = item["name"];
    const char *downloadUrl = item["download_url"];
    if (name == nullptr || downloadUrl == nullptr) {
      continue;
    }

    const String buildId = extractBuildId(String(name));
    if (!isNewerBuildId(buildId, artifact.buildId)) {
      continue;
    }

    artifact.buildId = buildId;
    artifact.downloadUrl = downloadUrl;
  }

  if (artifact.buildId.isEmpty()) {
    reportError(String("No valid release files found in ") + path);
    return false;
  }

  return true;
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
    reportError(String("Download failed for ") + artifact.buildId + ": HTTP " + httpCode);
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