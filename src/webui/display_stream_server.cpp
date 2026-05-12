#include "display_stream_server.h"

#include <Arduino.h>
#include <NetworkServer.h>

#include <cstring>

#include "../display/display_reader.h"

namespace {
constexpr uint16_t kDisplayStreamPort = 81;
constexpr uint32_t kDisplayStreamHeartbeatMs = 15000;

NetworkServer g_displayStreamServer(kDisplayStreamPort, 1);
NetworkClient g_displayStreamClient;
bool g_displayStreamClientConnected = false;
char g_lastStreamedDisplayText[9] = {0};
uint32_t g_lastDisplayStreamHeartbeatMs = 0;

void stopDisplayStreamClient() {
  if (g_displayStreamClientConnected) {
    g_displayStreamClient.stop();
  }
  g_displayStreamClientConnected = false;
  g_lastStreamedDisplayText[0] = '\0';
  g_lastDisplayStreamHeartbeatMs = 0;
}

void sendDisplayStreamComment(const char *comment) {
  if (!g_displayStreamClientConnected || !g_displayStreamClient.connected()) {
    stopDisplayStreamClient();
    return;
  }

  g_displayStreamClient.print(": ");
  g_displayStreamClient.print(comment);
  g_displayStreamClient.print("\n\n");
}

void sendDisplayStreamData(const char *displayText) {
  if (!g_displayStreamClientConnected || !g_displayStreamClient.connected()) {
    stopDisplayStreamClient();
    return;
  }

  g_displayStreamClient.print("event: display\n");
  g_displayStreamClient.print("data: ");
  g_displayStreamClient.print(displayText);
  g_displayStreamClient.print("\n\n");
  std::strncpy(g_lastStreamedDisplayText, displayText, sizeof(g_lastStreamedDisplayText) - 1U);
  g_lastStreamedDisplayText[sizeof(g_lastStreamedDisplayText) - 1U] = '\0';
  g_lastDisplayStreamHeartbeatMs = millis();
}

void startDisplayStreamClient(NetworkClient client) {
  stopDisplayStreamClient();

  client.setNoDelay(true);
  client.setSSE(true);
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: text/event-stream\r\n");
  client.print("Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n");
  client.print("Pragma: no-cache\r\n");
  client.print("Expires: 0\r\n");
  client.print("Connection: keep-alive\r\n");
  client.print("Access-Control-Allow-Origin: *\r\n");
  client.print("X-Accel-Buffering: no\r\n");
  client.print("\r\n");

  g_displayStreamClient = client;
  g_displayStreamClientConnected = true;
  g_lastStreamedDisplayText[0] = '\0';
  g_lastDisplayStreamHeartbeatMs = millis();

  sendDisplayStreamComment("connected");
  sendDisplayStreamData(getDisplaySnapshot().text);
}
}

void setupDisplayStreamServer() {
  g_displayStreamServer.begin();
  g_displayStreamServer.setNoDelay(true);
}

void displayStreamServerLoop() {
  if (g_displayStreamServer.hasClient()) {
    NetworkClient nextClient = g_displayStreamServer.accept();
    if (nextClient) {
      startDisplayStreamClient(nextClient);
    }
  }

  if (!g_displayStreamClientConnected) {
    return;
  }

  if (!g_displayStreamClient.connected()) {
    stopDisplayStreamClient();
    return;
  }

  while (g_displayStreamClient.available()) {
    g_displayStreamClient.read();
  }

  const DisplaySnapshot snapshot = getDisplaySnapshot();
  if (std::strncmp(g_lastStreamedDisplayText, snapshot.text, sizeof(g_lastStreamedDisplayText)) != 0) {
    sendDisplayStreamData(snapshot.text);
    return;
  }

  const uint32_t now = millis();
  if (now - g_lastDisplayStreamHeartbeatMs >= kDisplayStreamHeartbeatMs) {
    sendDisplayStreamComment("keepalive");
    g_lastDisplayStreamHeartbeatMs = now;
  }
}