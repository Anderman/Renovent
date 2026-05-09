#pragma once

#include <Arduino.h>

class PubSubClient;

bool publishDiscovery(PubSubClient &mqttClient, const String &nodeId);