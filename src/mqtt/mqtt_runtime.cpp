#include "mqtt/mqtt_runtime.h"

#include <PubSubClient.h>
#include <WiFi.h>

#include "mqtt/ha_entity_definitions.h"
#include "mqtt/mqtt_discovery_publisher.h"
#include "mqtt/mqtt_config.h"
#include "mqtt/mqtt_command_handler.h"
#include "mqtt/mqtt_state_publisher.h"
#include "mqtt/mqtt_topics.h"
#include "menu/setting_writer.h"
#include "input/key_writer.h"
#include "menu/sensors_menu.h"
#include "menu/settings_menu.h"
#include "core/ventilation_mode_state.h"

#include <cstdlib>
#include <cstring>

namespace
{

    constexpr unsigned long kReconnectIntervalMs = 5000;
    constexpr uint16_t kMqttBufferSize = 3072;
    constexpr uint32_t kVentilationModeHoldMs = 800;
    WiFiClient g_wifiClient;
    PubSubClient g_mqttClient(g_wifiClient);
    bool g_publishDiscoveryPending = true;
    unsigned long g_lastConnectAttemptMs = 0;

    void resetMqttSessionState()
    {
        g_publishDiscoveryPending = true;
        g_lastConnectAttemptMs = 0;
        mqttCommandHandlerQueueClear();
    }

    void disconnect()
    {
        if (g_mqttClient.connected())
        {
            g_mqttClient.disconnect();
        }
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

            const String topic = getCommandTopic(nodeId, definition->key);
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

    const HaEntityDefinition *findWritableCommandDefinition(const char *key)
    {
        if (key == nullptr || key[0] == '\0')
        {
            return nullptr;
        }

        const HaEntityDefinition *definition = findHaEntityDefinitionByKey(key);
        if (definition == nullptr || !definition->writable)
        {
            return nullptr;
        }

        return definition;
    }

    const HaEntityDefinition *tryResolveWritableCommandTopic(const char *topic, const String &nodeId)
    {
        if (topic == nullptr)
        {
            return nullptr;
        }

        const HaRootDefinition &root = getHaRootDefinition();
        const String topicPrefix = getRootTopic(nodeId) + "/" + root.commandTopicRoot + "/";
        if (!String(topic).startsWith(topicPrefix))
        {
            return nullptr;
        }

        const char *key = topic + topicPrefix.length();
        return findWritableCommandDefinition(key);
    }

    bool handleVentilationModeCommand(const char *payload)
    {
        uint8_t keyMask = static_cast<uint8_t>(kKeyOk | kKeyPlus);
        bool autoMode = false;

        if (std::strcmp(payload, "UIT") == 0)
        {
            keyMask = static_cast<uint8_t>(kKeyOk | kKeyMinus);
        }
        else if (std::strcmp(payload, "AUTO") == 0)
        {
            keyMask = static_cast<uint8_t>(kKeyOk | kKeyPlus);
            autoMode = true;
        }

        pulseKeys(keyMask, kVentilationModeHoldMs);
        ventilationModeStateSetAutoMode(autoMode);
        return true;
    }

    SettingWriteStatus handleSettingCommand(const HaEntityDefinition &definition, const char *payload)
    {
        if (definition.platform == HaEntityPlatform::Select)
        {
            for (size_t index = 0; index < definition.optionCount; ++index)
            {
                const HaSelectOptionDefinition &option = definition.options[index];
                if (std::strcmp(option.label, payload) != 0 && std::strcmp(option.value, payload) != 0)
                {
                    continue;
                }

                return writeSetting(definition.key, option.value);
            }

            return writeSetting(definition.key, payload);
        }

        int32_t value = 0;
        if (!tryParseNumber(definition, payload, value))
        {
            return SettingWriteStatus::InvalidKey;
        }

        return writeSetting(definition.key, value);
    }

    bool shouldRetryQueuedCommand(const MqttQueuedCommand &command)
    {
        const HaEntityDefinition *definition = findWritableCommandDefinition(command.key);
        if (definition == nullptr)
        {
            return false;
        }

        if (std::strcmp(definition->key, "ventilation_mode") == 0)
        {
            return !handleVentilationModeCommand(command.payload);
        }

        if (definition->sourceType != HaEntitySourceType::Setting)
        {
            return false;
        }

        const SettingWriteStatus status = handleSettingCommand(*definition, command.payload);
        return status != SettingWriteStatus::Scheduled && status != SettingWriteStatus::InvalidKey;
    }

    void processQueuedCommand()
    {
        if (commandExecutionBlocked())
        {
            return;
        }

        const MqttQueuedCommand *command = mqttCommandHandlerPeek();
        if (command == nullptr)
        {
            return;
        }

        if (shouldRetryQueuedCommand(*command))
        {
            return;
        }

        mqttCommandHandlerPop();
    }

    void mqttMessageCallback(char *topic, uint8_t *payloadBytes, unsigned int payloadLength)
    {
        char payload[32] = "";
        const size_t copyLength = payloadLength < sizeof(payload) - 1U ? payloadLength : sizeof(payload) - 1U;
        if (copyLength > 0)
        {
            std::memcpy(payload, payloadBytes, copyLength);
        }
        payload[copyLength] = '\0';

        const String nodeId = buildNodeId();
        const HaEntityDefinition *definition = tryResolveWritableCommandTopic(topic, nodeId);
        if (definition == nullptr)
        {
            return;
        }

        mqttCommandHandlerEnqueueLatest(definition->key, payload);
    }

    bool connectMqtt()
    {
        const MqttConfig &config = getMqttConfig();
        const String nodeId = buildNodeId();
        const String availabilityTopic = getAvailabilityTopic(nodeId);
        const String clientId = String("renovent-") + nodeId;
        const char *user = config.mqttUser.isEmpty() ? nullptr : config.mqttUser.c_str();
        const char *password = config.mqttUser.isEmpty() ? nullptr : config.mqttPassword.c_str();

        g_mqttClient.setServer(config.mqttHost.c_str(), config.mqttPort);
        g_mqttClient.setBufferSize(kMqttBufferSize);

        const bool connected = g_mqttClient.connect(clientId.c_str(), user, password, availabilityTopic.c_str(), 0, true, "offline");

        if (connected)
        {
            if (!subscribeWritableCommandTopics(nodeId))
            {
                disconnect();
                return false;
            }
            g_publishDiscoveryPending = true;
            return true;
        }

        return false;
    }

    bool runtimeSessionAllowed()
    {
        const MqttConfig &config = getMqttConfig();
        return WiFi.status() == WL_CONNECTED && !config.mqttHost.isEmpty() && config.mqttPort != 0;
    }

    bool ensureConnected()
    {
        if (g_mqttClient.connected())
        {
            return true;
        }

        const unsigned long now = millis();
        if (g_lastConnectAttemptMs != 0 && now - g_lastConnectAttemptMs < kReconnectIntervalMs)
        {
            return false;
        }

        g_lastConnectAttemptMs = now;
        return connectMqtt();
    }

    bool publishPendingMqttData(const String &nodeId)
    {
        const bool forcePublish = g_publishDiscoveryPending;

        if (g_publishDiscoveryPending)
        {
            if (!publishDiscovery(g_mqttClient, nodeId))
            {
                return false;
            }

            g_publishDiscoveryPending = false;
        }

        return publishStates(g_mqttClient, nodeId, forcePublish);
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
    if (!runtimeSessionAllowed())
    {
        disconnect();
        return;
    }

    if (!ensureConnected())
    {
        return;
    }

    g_mqttClient.loop();
    processQueuedCommand();

    if (publishPendingMqttData(buildNodeId()))
    {
        return;
    }

    disconnect();
}

void mqttRuntimeResetSession()
{
    resetMqttSessionState();
    disconnect();
}