#pragma once

#include <Arduino.h>

String getRootTopic(const String &nodeId);
String getAvailabilityTopic(const String &nodeId);
String getStateTopic(const String &nodeId, const char *key);
String getCommandTopic(const String &nodeId, const char *key);