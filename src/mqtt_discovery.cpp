#include "mqtt_discovery.h"

#include <PubSubClient.h>
#include <WiFi.h>

#include "co2_sensor.h"
#include "ha_entity_definitions.h"
#include "mqtt_config.h"
#include "mqtt_discovery_payload.h"
#include "ota/auto_update.h"
#include "setting_writer.h"
#include "key_writer.h"
#include "sensors_menu.h"
#include "settings_menu.h"

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>

namespace
{

    constexpr unsigned long kReconnectIntervalMs = 5000;
    constexpr uint16_t kMqttBufferSize = 3072;
    constexpr char kAvailabilityPayloadOnline[] = "online";
    constexpr char kAvailabilityPayloadOffline[] = "offline";
    constexpr uint32_t kVentilationModeHoldMs = 800;
    constexpr size_t kCommandQueueCapacity = 12;

    struct QueuedCommand
    {
        bool occupied = false;
        char key[24] = {0};
        char payload[32] = {0};
    };

    WiFiClient g_wifiClient;
    PubSubClient g_mqttClient(g_wifiClient);
    unsigned long g_lastConnectAttemptMs = 0;
    bool g_publishDiscoveryPending = true;
    bool g_publishCo2StatePending = true;
    bool g_publishSensorMenuStatePending = true;
    bool g_publishSettingsStatePending = true;
    bool g_publishStatusStatePending = true;
    uint32_t g_lastPublishedCo2SampleMs = 0;
    uint32_t g_lastPublishedSensorsMenuCompletedMs = 0;
    uint32_t g_lastPublishedSettingsCompletedMs = 0;
    int32_t g_lastPublishedRssi = 0;
    bool g_hasPublishedRssi = false;
    char g_lastVentilationModeLabel[8] = "";
    bool g_hasVentilationModeLabel = false;
    bool g_publishVentilationModeStatePending = false;
    QueuedCommand g_commandQueue[kCommandQueueCapacity];
    size_t g_commandQueueHead = 0;
    size_t g_commandQueueCount = 0;

    const char *platformToTopicSegment(HaEntityPlatform platform)
    {
        switch (platform)
        {
        case HaEntityPlatform::Sensor:
            return "sensor";
        case HaEntityPlatform::Number:
            return "number";
        case HaEntityPlatform::Select:
            return "select";
        }

        return "sensor";
    }

    String buildNodeId()
    {
        return String("node") + getMqttConfig().mqttNodeId;
    }

    String buildRenoventRootTopic(const String &nodeId)
    {
        return String("renovent/") + nodeId;
    }

    String buildAvailabilityTopic(const String &nodeId)
    {
        const HaRootDefinition &root = getHaRootDefinition();
        return buildRenoventRootTopic(nodeId) + "/" + root.availabilityTopicSuffix;
    }

    String buildDiscoveryTopic(const String &nodeId, const HaEntityDefinition &definition)
    {
        return String("homeassistant/") + platformToTopicSegment(definition.platform) + "/" + nodeId + "/" + definition.objectId + "/config";
    }

    String buildStateTopic(const String &nodeId, const char *key)
    {
        const HaRootDefinition &root = getHaRootDefinition();
        return buildRenoventRootTopic(nodeId) + "/" + root.stateTopicRoot + "/" + key;
    }

    String buildCommandTopic(const String &nodeId, const char *key)
    {
        const HaRootDefinition &root = getHaRootDefinition();
        return buildRenoventRootTopic(nodeId) + "/" + root.commandTopicRoot + "/" + key;
    }

    bool publishCo2Value(const String &nodeId, const char *key, const String &payload);

    bool queueIsFull()
    {
        return g_commandQueueCount >= kCommandQueueCapacity;
    }

    bool queueIsEmpty()
    {
        return g_commandQueueCount == 0;
    }

    void clearCommandQueue()
    {
        for (size_t index = 0; index < kCommandQueueCapacity; ++index)
        {
            g_commandQueue[index] = QueuedCommand{};
        }

        g_commandQueueHead = 0;
        g_commandQueueCount = 0;
    }

    void rebuildQueueWithoutKey(const char *key)
    {
        if (key == nullptr || queueIsEmpty())
        {
            return;
        }

        QueuedCommand filteredQueue[kCommandQueueCapacity] = {};
        size_t filteredCount = 0;

        for (size_t offset = 0; offset < g_commandQueueCount; ++offset)
        {
            const size_t index = (g_commandQueueHead + offset) % kCommandQueueCapacity;
            const QueuedCommand &entry = g_commandQueue[index];
            if (!entry.occupied || std::strcmp(entry.key, key) == 0)
            {
                continue;
            }

            filteredQueue[filteredCount++] = entry;
        }

        clearCommandQueue();
        for (size_t index = 0; index < filteredCount; ++index)
        {
            g_commandQueue[index] = filteredQueue[index];
        }
        g_commandQueueCount = filteredCount;
    }

    bool enqueueCommand(const char *key, const char *payload)
    {
        if (key == nullptr || payload == nullptr)
        {
            return false;
        }

        rebuildQueueWithoutKey(key);
        if (queueIsFull())
        {
            return false;
        }

        const size_t tail = (g_commandQueueHead + g_commandQueueCount) % kCommandQueueCapacity;
        QueuedCommand &entry = g_commandQueue[tail];
        entry = QueuedCommand{};
        entry.occupied = true;
        std::strncpy(entry.key, key, sizeof(entry.key) - 1);
        entry.key[sizeof(entry.key) - 1] = '\0';
        std::strncpy(entry.payload, payload, sizeof(entry.payload) - 1);
        entry.payload[sizeof(entry.payload) - 1] = '\0';
        ++g_commandQueueCount;
        return true;
    }

    const QueuedCommand *peekQueuedCommand()
    {
        if (queueIsEmpty())
        {
            return nullptr;
        }

        return &g_commandQueue[g_commandQueueHead];
    }

    void popQueuedCommand()
    {
        if (queueIsEmpty())
        {
            return;
        }

        g_commandQueue[g_commandQueueHead] = QueuedCommand{};
        g_commandQueueHead = (g_commandQueueHead + 1U) % kCommandQueueCapacity;
        --g_commandQueueCount;
    }

    bool commandExecutionBlocked()
    {
        return sensorsMenuIsBusy() || settingsMenuIsBusy() || settingWriterIsBusy();
    }

    bool isNumericSettingKey(const char *key)
    {
        if (key == nullptr)
        {
            return false;
        }

        return key[0] == 'U' || key[0] == 'I';
    }

    bool tryGetOptionValueForLabel(const HaEntityDefinition &definition,
                                   const char *label,
                                   int32_t &value)
    {
        if (label == nullptr)
        {
            return false;
        }

        for (size_t index = 0; index < definition.optionCount; ++index)
        {
            const HaSelectOptionDefinition &option = definition.options[index];
            if (std::strcmp(option.label, label) != 0 && std::strcmp(option.value, label) != 0)
            {
                continue;
            }

            value = std::atoi(option.value);
            return true;
        }

        return false;
    }

    bool parseIntegerPayload(const char *payload, int32_t &value)
    {
        if (payload == nullptr || payload[0] == '\0')
        {
            return false;
        }

        char *end = nullptr;
        const long parsed = std::strtol(payload, &end, 10);
        if (end == payload)
        {
            return false;
        }

        while (end != nullptr && *end != '\0')
        {
            if (!std::isspace(static_cast<unsigned char>(*end)))
            {
                return false;
            }
            ++end;
        }

        value = static_cast<int32_t>(parsed);
        return true;
    }

    int32_t numberScaleFactor(const HaEntityDefinition &definition)
    {
        if (definition.suggestedDisplayPrecision <= 0)
        {
            return 1;
        }

        int32_t factor = 1;
        for (int8_t index = 0; index < definition.suggestedDisplayPrecision; ++index)
        {
            factor *= 10;
        }

        return factor;
    }

    bool parseNumberPayload(const HaEntityDefinition &definition, const char *payload, int32_t &value)
    {
        if (payload == nullptr || payload[0] == '\0')
        {
            return false;
        }

        const int32_t scaleFactor = numberScaleFactor(definition);
        if (scaleFactor == 1)
        {
            return parseIntegerPayload(payload, value);
        }

        char *end = nullptr;
        const double parsed = std::strtod(payload, &end);
        if (end == payload)
        {
            return false;
        }

        while (end != nullptr && *end != '\0')
        {
            if (!std::isspace(static_cast<unsigned char>(*end)))
            {
                return false;
            }
            ++end;
        }

        value = static_cast<int32_t>(std::lround(parsed * static_cast<double>(scaleFactor)));
        return true;
    }

    String formatNumberStatePayload(const HaEntityDefinition &definition, int32_t rawValue)
    {
        const int32_t scaleFactor = numberScaleFactor(definition);
        if (scaleFactor == 1)
        {
            return String(rawValue);
        }

        return String(static_cast<double>(rawValue) / static_cast<double>(scaleFactor),
                      definition.suggestedDisplayPrecision);
    }

    bool publishVentilationModeState(const String &nodeId)
    {
        if (!g_hasVentilationModeLabel)
        {
            return true;
        }

        return publishCo2Value(nodeId, "ventilation_mode", String(g_lastVentilationModeLabel));
    }

    bool publishVirtualStatesIfNeeded()
    {
        if (!g_publishVentilationModeStatePending)
        {
            return true;
        }

        const String nodeId = buildNodeId();
        if (!publishVentilationModeState(nodeId))
        {
            return false;
        }

        g_publishVentilationModeStatePending = false;
        return true;
    }

    bool subscribeWritableCommandTopics()
    {
        const String nodeId = buildNodeId();
        for (size_t index = 0; index < getHaEntityDefinitionCount(); ++index)
        {
            const HaEntityDefinition *definition = getHaEntityDefinitionAt(index);
            if (definition == nullptr || !definition->writable)
            {
                continue;
            }

            if (!g_mqttClient.subscribe(buildCommandTopic(nodeId, definition->key).c_str()))
            {
                return false;
            }
        }

        return true;
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
        std::strncpy(g_lastVentilationModeLabel, stateLabel, sizeof(g_lastVentilationModeLabel) - 1);
        g_lastVentilationModeLabel[sizeof(g_lastVentilationModeLabel) - 1] = '\0';
        g_hasVentilationModeLabel = true;
        g_publishVentilationModeStatePending = true;
        return true;
    }

    bool handleSettingCommand(const HaEntityDefinition &definition, const char *payload)
    {
        int32_t value = 0;
        if (definition.platform == HaEntityPlatform::Select)
        {
            if (!tryGetOptionValueForLabel(definition, payload, value))
            {
                value = std::atoi(payload);
            }
        }
        else if (!parseNumberPayload(definition, payload, value))
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

        const QueuedCommand *command = peekQueuedCommand();
        if (command == nullptr)
        {
            return false;
        }

        const HaEntityDefinition *definition = findHaEntityDefinitionByKey(command->key);
        if (definition == nullptr || !definition->writable)
        {
            Serial.print("[mqtt] dropped invalid queued command: ");
            Serial.println(command->key);
            popQueuedCommand();
            return true;
        }

        const bool handled = std::strcmp(definition->key, "ventilation_mode") == 0
                                 ? handleVentilationModeCommand(command->payload)
                                 : (isNumericSettingKey(definition->key) && handleSettingCommand(*definition, command->payload));

        if (!handled)
        {
            return false;
        }

        Serial.print("[mqtt] command dequeued ");
        Serial.print(definition->key);
        Serial.print(": ");
        Serial.println(command->payload);
        popQueuedCommand();
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
        const String topicPrefix = buildRenoventRootTopic(nodeId) + "/" + root.commandTopicRoot + "/";
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

        Serial.print("[mqtt] command ");
        Serial.print(definition->key);
        Serial.print(enqueueCommand(definition->key, payload) ? " queued: " : " dropped: ");
        Serial.println(payload);
    }

    bool publishEntityDiscovery(const String &nodeId,
                                const String &availabilityTopic,
                                const String &firmwareBuildId,
                                const HaEntityDefinition &definition)
    {
        String payload;
        if (!buildHaDiscoveryPayload(payload, nodeId, availabilityTopic, firmwareBuildId, definition))
        {
            return false;
        }

        const String topic = buildDiscoveryTopic(nodeId, definition);
        return g_mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }

    bool publishDiscoveryPayloads()
    {
        const String nodeId = buildNodeId();
        const String availabilityTopic = buildAvailabilityTopic(nodeId);
        const String firmwareBuildId = getCurrentFirmwareBuildId();

        if (!g_mqttClient.publish(availabilityTopic.c_str(), kAvailabilityPayloadOnline, true))
        {
            return false;
        }

        for (size_t index = 0; index < getHaEntityDefinitionCount(); ++index)
        {
            const HaEntityDefinition *definition = getHaEntityDefinitionAt(index);
            if (definition == nullptr)
            {
                continue;
            }

            if (!publishEntityDiscovery(nodeId, availabilityTopic, firmwareBuildId, *definition))
            {
                return false;
            }
        }

        return true;
    }

    bool publishCo2Value(const String &nodeId, const char *key, const String &payload)
    {
        const String topic = buildStateTopic(nodeId, key);
        return g_mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }

    bool publishCo2StatesIfNeeded()
    {
        const Co2SensorStatus status = getCo2SensorStatus();
        if (!status.dataValid)
        {
            return true;
        }

        if (!g_publishCo2StatePending && status.lastSampleMs == g_lastPublishedCo2SampleMs)
        {
            return true;
        }

        const String nodeId = buildNodeId();
        if (!publishCo2Value(nodeId, "co2_ppm", String(status.co2Ppm)))
        {
            return false;
        }

        if (!publishCo2Value(nodeId, "co2_temperature", String(status.temperatureC, 1)))
        {
            return false;
        }

        if (!publishCo2Value(nodeId, "co2_humidity", String(status.humidityPct, 1)))
        {
            return false;
        }

        g_lastPublishedCo2SampleMs = status.lastSampleMs;
        g_publishCo2StatePending = false;
        return true;
    }

    bool tryGetSensorMenuValueByKey(const SensorsMenuStatus &status, const char *key, int32_t &value)
    {
        for (uint8_t index = 1; index <= 14; ++index)
        {
            const SensorsMenuValueDefinition definition = getSensorsMenuValueDefinition(index);
            if (std::strcmp(definition.key, key) != 0)
            {
                continue;
            }

            const SensorsMenuValueItem &item = status.values[index - 1U];
            if (!item.available || !item.hasValue)
            {
                return false;
            }

            value = item.value;
            return true;
        }

        return false;
    }

    bool tryGetSettingsValueByKey(const SettingsMenuStatus &status, const char *key, SettingsMenuValue &value)
    {
        for (uint8_t index = 0; index < status.count; ++index)
        {
            SettingsMenuValue currentValue{};
            if (!getSettingsMenuValue(index, currentValue))
            {
                continue;
            }

            if (std::strcmp(currentValue.key, key) == 0)
            {
                value = currentValue;
                return true;
            }
        }

        return false;
    }

    bool publishIntegerValue(const String &nodeId, const char *key, int32_t value)
    {
        return publishCo2Value(nodeId, key, String(value));
    }

    bool publishNumberValue(const String &nodeId,
                            const HaEntityDefinition &definition,
                            int32_t rawValue)
    {
        return publishCo2Value(nodeId, definition.key, formatNumberStatePayload(definition, rawValue));
    }

    bool publishSelectValue(const String &nodeId,
                            const HaEntityDefinition &definition,
                            int32_t rawValue)
    {
        for (size_t index = 0; index < definition.optionCount; ++index)
        {
            const HaSelectOptionDefinition &option = definition.options[index];
            if (std::atoi(option.value) != rawValue)
            {
                continue;
            }

            return publishCo2Value(nodeId, definition.key, option.label);
        }

        return false;
    }

    bool publishSensorMenuStatesIfNeeded()
    {
        const SensorsMenuStatus status = getSensorsMenuStatus();
        if (status.lastCompletedMs == 0)
        {
            return true;
        }

        if (!g_publishSensorMenuStatePending && status.lastCompletedMs == g_lastPublishedSensorsMenuCompletedMs)
        {
            return true;
        }

        const String nodeId = buildNodeId();
        for (size_t index = 0; index < getHaEntityDefinitionCount(); ++index)
        {
            const HaEntityDefinition *definition = getHaEntityDefinitionAt(index);
            if (definition == nullptr || definition->sourceType != HaEntitySourceType::SensorMenu)
            {
                continue;
            }

            int32_t value = 0;
            if (!tryGetSensorMenuValueByKey(status, definition->key, value))
            {
                continue;
            }

            if (!publishIntegerValue(nodeId, definition->key, value))
            {
                return false;
            }
        }

        g_lastPublishedSensorsMenuCompletedMs = status.lastCompletedMs;
        g_publishSensorMenuStatePending = false;
        return true;
    }

    bool publishSettingsStatesIfNeeded()
    {
        const SettingsMenuStatus status = getSettingsMenuStatus();
        if (status.lastCompletedMs == 0)
        {
            return true;
        }

        if (!g_publishSettingsStatePending && status.lastCompletedMs == g_lastPublishedSettingsCompletedMs)
        {
            return true;
        }

        const String nodeId = buildNodeId();
        for (size_t index = 0; index < getHaEntityDefinitionCount(); ++index)
        {
            const HaEntityDefinition *definition = getHaEntityDefinitionAt(index);
            if (definition == nullptr || definition->sourceType != HaEntitySourceType::Setting)
            {
                continue;
            }

            SettingsMenuValue value{};
            if (!tryGetSettingsValueByKey(status, definition->key, value) || !value.hasValue)
            {
                continue;
            }

            const bool published = definition->platform == HaEntityPlatform::Select
                ? publishSelectValue(nodeId, *definition, value.value)
                : publishNumberValue(nodeId, *definition, value.value);
            if (!published)
            {
                return false;
            }
        }

        g_lastPublishedSettingsCompletedMs = status.lastCompletedMs;
        g_publishSettingsStatePending = false;
        return true;
    }

    bool publishStatusStatesIfNeeded()
    {
        const int32_t rssi = WiFi.RSSI();
        if (!g_publishStatusStatePending && g_hasPublishedRssi && rssi == g_lastPublishedRssi)
        {
            return true;
        }

        const String nodeId = buildNodeId();
        if (!publishIntegerValue(nodeId, "rssi", rssi))
        {
            return false;
        }

        g_lastPublishedRssi = rssi;
        g_hasPublishedRssi = true;
        g_publishStatusStatePending = false;
        return true;
    }

    bool connectMqtt()
    {
        const MqttConfig &config = getMqttConfig();
        if (config.mqttHost.isEmpty() || config.mqttPort == 0 || WiFi.status() != WL_CONNECTED)
        {
            return false;
        }

        const String nodeId = buildNodeId();
        const String availabilityTopic = buildAvailabilityTopic(nodeId);
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
            Serial.print("[mqtt] connected to ");
            Serial.println(config.mqttHost);
            if (!subscribeWritableCommandTopics())
            {
                Serial.println("[mqtt] subscribe failed");
                g_mqttClient.disconnect();
                return false;
            }
            g_publishDiscoveryPending = true;
            g_publishCo2StatePending = true;
            g_publishSensorMenuStatePending = true;
            g_publishSettingsStatePending = true;
            g_publishStatusStatePending = true;
            g_publishVentilationModeStatePending = g_hasVentilationModeLabel;
            return true;
        }

        Serial.print("[mqtt] connect failed, rc=");
        Serial.println(g_mqttClient.state());
        return false;
    }

} // namespace

void mqttDiscoverySetup()
{
    g_mqttClient.setClient(g_wifiClient);
    g_mqttClient.setCallback(mqttMessageCallback);
    g_publishDiscoveryPending = true;
    g_publishCo2StatePending = true;
    g_publishSensorMenuStatePending = true;
    g_publishSettingsStatePending = true;
    g_publishStatusStatePending = true;
    g_publishVentilationModeStatePending = true;
    g_lastConnectAttemptMs = 0;
    g_lastPublishedCo2SampleMs = 0;
    g_lastPublishedSensorsMenuCompletedMs = 0;
    g_lastPublishedSettingsCompletedMs = 0;
    g_lastPublishedRssi = 0;
    g_hasPublishedRssi = false;
    std::strncpy(g_lastVentilationModeLabel, "AAN", sizeof(g_lastVentilationModeLabel) - 1);
    g_lastVentilationModeLabel[sizeof(g_lastVentilationModeLabel) - 1] = '\0';
    g_hasVentilationModeLabel = true;
    clearCommandQueue();
}

void mqttDiscoveryLoop()
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

    if (g_publishDiscoveryPending)
    {
        if (publishDiscoveryPayloads())
        {
            Serial.println("[mqtt] discovery published");
            g_publishDiscoveryPending = false;
        }
        else
        {
            Serial.println("[mqtt] discovery publish failed");
            g_mqttClient.disconnect();
            return;
        }
    }

    if (publishCo2StatesIfNeeded())
    {
        if (publishSensorMenuStatesIfNeeded())
        {
            if (publishSettingsStatesIfNeeded())
            {
                if (publishStatusStatesIfNeeded())
                {
                    if (publishVirtualStatesIfNeeded())
                    {
                        return;
                    }

                    Serial.println("[mqtt] virtual state publish failed");
                    g_mqttClient.disconnect();
                    return;
                }

                Serial.println("[mqtt] status state publish failed");
                g_mqttClient.disconnect();
                return;
            }

            Serial.println("[mqtt] settings state publish failed");
            g_mqttClient.disconnect();
            return;
        }

        Serial.println("[mqtt] sensors state publish failed");
        g_mqttClient.disconnect();
        return;
    }

    Serial.println("[mqtt] CO2 state publish failed");
    g_mqttClient.disconnect();
}

void mqttDiscoveryConfigChanged()
{
    g_publishDiscoveryPending = true;
    g_publishCo2StatePending = true;
    g_publishSensorMenuStatePending = true;
    g_publishSettingsStatePending = true;
    g_publishStatusStatePending = true;
    g_publishVentilationModeStatePending = g_hasVentilationModeLabel;
    g_lastConnectAttemptMs = 0;
    g_lastPublishedCo2SampleMs = 0;
    g_lastPublishedSensorsMenuCompletedMs = 0;
    g_lastPublishedSettingsCompletedMs = 0;
    g_lastPublishedRssi = 0;
    g_hasPublishedRssi = false;
    clearCommandQueue();
    if (g_mqttClient.connected())
    {
        g_mqttClient.disconnect();
    }
}