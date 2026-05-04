#pragma once

#include <Arduino.h>

String mqttBuildRenoventRootTopic(const String &nodeId);
String mqttBuildAvailabilityTopic(const String &nodeId);
String mqttBuildStateTopic(const String &nodeId, const char *key);
String mqttBuildCommandTopic(const String &nodeId, const char *key);