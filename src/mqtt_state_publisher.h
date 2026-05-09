#pragma once

#include <Arduino.h>

class PubSubClient;

bool publishStates(PubSubClient &mqttClient,
                   const String &nodeId,
                   bool forcePublish);