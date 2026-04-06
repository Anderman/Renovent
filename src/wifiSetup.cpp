#include "wifiSetup.h"

#include <WiFi.h>
#include <WiFiManager.h>

namespace {
WiFiManager g_wifiManager;
}

void setupWifi() {
  WiFi.setHostname("renovent");
  WiFi.setAutoReconnect(true);

  if (WiFi.status() != WL_CONNECTED) {
    g_wifiManager.autoConnect("Renovent-Setup");
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