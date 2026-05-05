#pragma once

#include <Arduino.h>

class PubSubClient;

bool mqttDiscoveryPublisherPublish(PubSubClient &mqttClient,
                                   const String &nodeId,
                                   const String &availabilityTopic,
                                   const char *availabilityPayloadOnline);