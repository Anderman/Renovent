#pragma once

#include <ArduinoJson.h>
#include <WebServer.h>

bool parseJsonBody(JsonDocument &doc);
void setupWebUi();
void webUiLoop();
extern WebServer server;