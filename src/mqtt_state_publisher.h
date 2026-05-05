#pragma once

#include <Arduino.h>

class PubSubClient;

bool mqttStatePublisherPublishAllStates(PubSubClient &mqttClient,
                                        const String &nodeId);