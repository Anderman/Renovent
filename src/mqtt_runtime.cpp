#include "mqtt_runtime.h"

#include <PubSubClient.h>
#include <WiFi.h>

#include "ha_entity_definitions.h"
#include "mqtt_config.h"
#include "mqtt_command_handler.h"
#include "mqtt_topics.h"
#include "mqtt_writer.h"
#include "setting_writer.h"
#include "key_writer.h"
#include "sensors_menu.h"
#include "settings_menu.h"

#include <cstdlib>
#include <cstring>

namespace
{

    constexpr unsigned long kReconnectIntervalMs = 5000;
    constexpr uint16_t kMqttBufferSize = 3072;
    constexpr char kAvailabilityPayloadOnline[] = "online";
    constexpr char kAvailabilityPayloadOffline[] = "offline";
    constexpr uint32_t kVentilationModeHoldMs = 800;
    WiFiClient g_wifiClient;
    PubSubClient g_mqttClient(g_wifiClient);
    unsigned long g_lastConnectAttemptMs = 0;
    MqttWriterState g_writerState;

    void resetMqttSessionState()
    {
        mqttWriterResetState(g_writerState);
        g_lastConnectAttemptMs = 0;
        mqttCommandHandlerQueueClear();
    }

    String buildNodeId()
    {
        return String("node") + getMqttConfig().mqttNodeId;
    }

    bool subscribeWritableCommandTopics(const String &nodeId)
    {
        for (size_t index = 0; index < getHaEntityDefinitionCount(); ++index)
        {
            const HaEntityDefinition *definition = getHaEntityDefinitionAt(index);
            if (definition == nullptr || !definition->writable)
            {
                continue;
            }

            const String topic = mqttBuildCommandTopic(nodeId, definition->key);
            if (!g_mqttClient.subscribe(topic.c_str()))
            {
                return false;
            }
        }

        return true;
    }

    bool commandExecutionBlocked()
    {
        return sensorsMenuIsBusy() || settingsMenuIsBusy() || settingWriterIsBusy();
    }

    bool handleVentilationModeCommand(const char *payload)
    {
        uint8_t keyMask = static_cast<uint8_t>(kKeyOk | kKeyPlus);
        const char *stateLabel = "AAN";

        if (std::strcmp(payload, "UIT") == 0)
        {
            keyMask = static_cast<uint8_t>(kKeyOk | kKeyMinus);
            stateLabel = "UIT";
        }
        else if (std::strcmp(payload, "AUTO") == 0)
        {
            keyMask = static_cast<uint8_t>(kKeyOk | kKeyPlus);
            stateLabel = "AAN";
        }

        pulseKeys(keyMask, kVentilationModeHoldMs);
        mqttWriterSetVentilationModeLabel(g_writerState, stateLabel);
        return true;
    }

    bool handleSettingCommand(const HaEntityDefinition &definition, const char *payload)
    {
        int32_t value = 0;
        if (definition.platform == HaEntityPlatform::Select)
        {
            if (!mqttCommandHandlerTryGetOptionValueForLabel(definition, payload, value))
            {
                value = std::atoi(payload);
            }
        }
        else if (!mqttCommandHandlerParseNumberPayload(definition, payload, value))
        {
            return false;
        }

        return requestSettingWrite(definition.key, value);
    }

    bool tryProcessQueuedCommand()
    {
        if (commandExecutionBlocked())
        {
            return false;
        }

        const MqttQueuedCommand *command = mqttCommandHandlerPeek();
        if (command == nullptr)
        {
            return false;
        }

        const HaEntityDefinition *definition = findHaEntityDefinitionByKey(command->key);
        if (definition == nullptr || !definition->writable)
        {
            mqttCommandHandlerPop();
            return true;
        }

        const bool handled = std::strcmp(definition->key, "ventilation_mode") == 0
                                 ? handleVentilationModeCommand(command->payload)
                                 : (mqttCommandHandlerIsNumericSettingKey(definition->key) && handleSettingCommand(*definition, command->payload));

        if (!handled)
        {
            return false;
        }

        mqttCommandHandlerPop();
        return true;
    }

    void mqttMessageCallback(char *topic, uint8_t *payloadBytes, unsigned int payloadLength)
    {
        if (topic == nullptr)
        {
            return;
        }

        char payload[32] = "";
        const size_t copyLength = payloadLength < sizeof(payload) - 1U ? payloadLength : sizeof(payload) - 1U;
        if (copyLength > 0)
        {
            std::memcpy(payload, payloadBytes, copyLength);
        }
        payload[copyLength] = '\0';

        const String nodeId = buildNodeId();
        const HaRootDefinition &root = getHaRootDefinition();
        const String topicPrefix = mqttBuildRenoventRootTopic(nodeId) + "/" + root.commandTopicRoot + "/";
        if (!String(topic).startsWith(topicPrefix))
        {
            return;
        }

        const char *key = topic + topicPrefix.length();
        if (key[0] == '\0')
        {
            return;
        }

        const HaEntityDefinition *definition = findHaEntityDefinitionByKey(key);
        if (definition == nullptr || !definition->writable)
        {
            return;
        }

        mqttCommandHandlerEnqueueLatest(definition->key, payload);
    }

    bool connectMqtt()
    {
        const MqttConfig &config = getMqttConfig();
        if (config.mqttHost.isEmpty() || config.mqttPort == 0 || WiFi.status() != WL_CONNECTED)
        {
            return false;
        }

        const String nodeId = buildNodeId();
        const String availabilityTopic = mqttBuildAvailabilityTopic(nodeId);
        const String clientId = String("renovent-") + nodeId;

        g_mqttClient.setServer(config.mqttHost.c_str(), config.mqttPort);
        g_mqttClient.setBufferSize(kMqttBufferSize);

        const bool connected = config.mqttUser.isEmpty()
                                   ? g_mqttClient.connect(clientId.c_str(), availabilityTopic.c_str(), 0, true, kAvailabilityPayloadOffline)
                                   : g_mqttClient.connect(clientId.c_str(),
                                                          config.mqttUser.c_str(),
                                                          config.mqttPassword.c_str(),
                                                          availabilityTopic.c_str(),
                                                          0,
                                                          true,
                                                          kAvailabilityPayloadOffline);

        if (connected)
        {
            if (!subscribeWritableCommandTopics(nodeId))
            {
                g_mqttClient.disconnect();
                return false;
            }
            g_writerState.publishDiscoveryPending = true;
            g_writerState.publishCo2StatePending = true;
            g_writerState.publishSensorMenuStatePending = true;
            g_writerState.publishSettingsStatePending = true;
            g_writerState.publishStatusStatePending = true;
            g_writerState.publishVentilationModeStatePending = g_writerState.hasVentilationModeLabel;
            return true;
        }

        return false;
    }

} // namespace

void mqttRuntimeSetup()
{
    g_mqttClient.setClient(g_wifiClient);
    g_mqttClient.setCallback(mqttMessageCallback);
    resetMqttSessionState();
}

void mqttRuntimeLoop()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        if (g_mqttClient.connected())
        {
            g_mqttClient.disconnect();
        }
        return;
    }

    const MqttConfig &config = getMqttConfig();
    if (config.mqttHost.isEmpty() || config.mqttPort == 0)
    {
        if (g_mqttClient.connected())
        {
            g_mqttClient.disconnect();
        }
        return;
    }

    if (!g_mqttClient.connected())
    {
        const unsigned long now = millis();
        if (g_lastConnectAttemptMs != 0 && now - g_lastConnectAttemptMs < kReconnectIntervalMs)
        {
            return;
        }

        g_lastConnectAttemptMs = now;
        if (!connectMqtt())
        {
            return;
        }
    }

    g_mqttClient.loop();

    tryProcessQueuedCommand();

    const String nodeId = buildNodeId();

    if (g_writerState.publishDiscoveryPending)
    {
        if (mqttWriterPublishDiscoveryPayloads(g_mqttClient,
                                               nodeId,
                                               mqttBuildAvailabilityTopic(nodeId),
                                               kAvailabilityPayloadOnline))
        {
            g_writerState.publishDiscoveryPending = false;
        }
        else
        {
            g_mqttClient.disconnect();
            return;
        }
    }

    if (mqttWriterPublishCo2StatesIfNeeded(g_mqttClient, nodeId, g_writerState))
    {
        if (mqttWriterPublishSensorMenuStatesIfNeeded(g_mqttClient, nodeId, g_writerState))
        {
            if (mqttWriterPublishSettingsStatesIfNeeded(g_mqttClient, nodeId, g_writerState))
            {
                if (mqttWriterPublishStatusStatesIfNeeded(g_mqttClient, nodeId, g_writerState))
                {
                    if (mqttWriterPublishVirtualStatesIfNeeded(g_mqttClient, nodeId, g_writerState))
                    {
                        return;
                    }

                    g_mqttClient.disconnect();
                    return;
                }

                g_mqttClient.disconnect();
                return;
            }

            g_mqttClient.disconnect();
            return;
        }

        g_mqttClient.disconnect();
        return;
    }

    g_mqttClient.disconnect();
}

void mqttRuntimeResetSession()
{
    resetMqttSessionState();
    if (g_mqttClient.connected())
    {
        g_mqttClient.disconnect();
    }
}