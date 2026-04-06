#pragma once

#include <Arduino.h>

struct MqttConfig {
  String mqttHost;
  uint16_t mqttPort = 1883;
  String mqttUser;
  String mqttPassword;
};

void mqttConfigSetup();
const MqttConfig &getMqttConfig();
bool updateMqttConfig(const MqttConfig &config);
