#pragma once

#include <Arduino.h>

class PubSubClient;

struct MqttWriterState
{
    bool publishDiscoveryPending = true;
    bool publishCo2StatePending = true;
    bool publishSensorMenuStatePending = true;
    bool publishSettingsStatePending = true;
    bool publishStatusStatePending = true;
    uint32_t lastPublishedCo2SampleMs = 0;
    uint32_t lastPublishedSensorsMenuCompletedMs = 0;
    uint32_t lastPublishedSettingsCompletedMs = 0;
    int32_t lastPublishedRssi = 0;
    bool hasPublishedRssi = false;
    char lastVentilationModeLabel[8] = "";
    bool hasVentilationModeLabel = false;
    bool publishVentilationModeStatePending = true;
};

void mqttWriterResetState(MqttWriterState &state);
void mqttWriterSetVentilationModeLabel(MqttWriterState &state, const char *label);
bool mqttWriterPublishDiscoveryPayloads(PubSubClient &mqttClient,
                                        const String &nodeId,
                                        const String &availabilityTopic,
                                        const char *availabilityPayloadOnline);
bool mqttWriterPublishCo2StatesIfNeeded(PubSubClient &mqttClient,
                                        const String &nodeId,
                                        MqttWriterState &state);
bool mqttWriterPublishSensorMenuStatesIfNeeded(PubSubClient &mqttClient,
                                               const String &nodeId,
                                               MqttWriterState &state);
bool mqttWriterPublishSettingsStatesIfNeeded(PubSubClient &mqttClient,
                                             const String &nodeId,
                                             MqttWriterState &state);
bool mqttWriterPublishStatusStatesIfNeeded(PubSubClient &mqttClient,
                                           const String &nodeId,
                                           MqttWriterState &state);
bool mqttWriterPublishVirtualStatesIfNeeded(PubSubClient &mqttClient,
                                            const String &nodeId,
                                            MqttWriterState &state);