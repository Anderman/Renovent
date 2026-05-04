#include "wifiSetup.h"

#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_wifi.h>

namespace {
WiFiManager g_wifiManager;

struct SavedWifiCredentials {
  String ssid;
  String password;
};

SavedWifiCredentials loadSavedWifiCredentials() {
  wifi_config_t wifiConfig = {};
  if (esp_wifi_get_config(WIFI_IF_STA, &wifiConfig) != ESP_OK) {
    return {};
  }

  SavedWifiCredentials credentials;
  credentials.ssid = reinterpret_cast<const char *>(wifiConfig.sta.ssid);
  credentials.password = reinterpret_cast<const char *>(wifiConfig.sta.password);
  return credentials;
}

bool connectToStrongestSavedAccessPoint() {
  const SavedWifiCredentials credentials = loadSavedWifiCredentials();
  if (credentials.ssid.isEmpty()) {
    return false;
  }

  const int networkCount = WiFi.scanNetworks(false, true);
  if (networkCount <= 0) {
    return false;
  }

  int bestNetworkIndex = -1;
  int bestRssi = INT32_MIN;
  for (int index = 0; index < networkCount; ++index) {
    if (WiFi.SSID(index) != credentials.ssid) {
      continue;
    }

    const int currentRssi = WiFi.RSSI(index);
    if (bestNetworkIndex >= 0 && currentRssi <= bestRssi) {
      continue;
    }

    bestNetworkIndex = index;
    bestRssi = currentRssi;
  }

  if (bestNetworkIndex < 0) {
    WiFi.scanDelete();
    return false;
  }

  uint8_t bssid[6] = {};
  WiFi.BSSID(bestNetworkIndex, bssid);
  const int32_t channel = WiFi.channel(bestNetworkIndex);

  Serial.printf("[renovent] wifi: connecting to strongest AP for %s on channel %ld (RSSI %d)\n",
                credentials.ssid.c_str(),
                static_cast<long>(channel),
                bestRssi);

  WiFi.begin(credentials.ssid.c_str(), credentials.password.c_str(), channel, bssid, true);

  const uint32_t connectStartedMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - connectStartedMs < 15000) {
    delay(100);
  }

  WiFi.scanDelete();
  return WiFi.status() == WL_CONNECTED;
}
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("renovent");
  WiFi.setAutoReconnect(true);

  if (WiFi.status() != WL_CONNECTED) {
    if (!connectToStrongestSavedAccessPoint()) {
      Serial.println("[renovent] wifi: strongest-AP connect failed, falling back to WiFiManager");
      g_wifiManager.autoConnect("Renovent-Setup");
    }
  }
}

void setupWifiConfigPage() {
  g_wifiManager.setConfigPortalBlocking(false);
  g_wifiManager.setHttpPort(8080);
  g_wifiManager.startWebPortal();
}

void wifiManagerLoop() {
  g_wifiManager.process();
}