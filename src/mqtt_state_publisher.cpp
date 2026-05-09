#include "mqtt_state_publisher.h"

#include <PubSubClient.h>
#include <WiFi.h>

#include "co2_sensor.h"
#include "ha_entity_definitions.h"
#include "mqtt_topics.h"
#include "sensors_menu.h"
#include "setting_writer.h"
#include "settings_menu.h"
#include "ventilation_mode_state.h"

#include <cstdlib>
#include <cstring>

namespace
{
    constexpr size_t kPublishedStateCacheCapacity = 64;

    struct PublishedStateCacheEntry
    {
        bool occupied = false;
        char key[32] = {0};
        char payload[64] = {0};
    };

    PublishedStateCacheEntry g_publishedStateCache[kPublishedStateCacheCapacity];

    PublishedStateCacheEntry *findPublishedStateCacheEntry(const char *key)
    {
        if (key == nullptr || key[0] == '\0')
        {
            return nullptr;
        }

        for (size_t index = 0; index < kPublishedStateCacheCapacity; ++index)
        {
            PublishedStateCacheEntry &entry = g_publishedStateCache[index];
            if (entry.occupied && std::strcmp(entry.key, key) == 0)
            {
                return &entry;
            }
        }

        for (size_t index = 0; index < kPublishedStateCacheCapacity; ++index)
        {
            PublishedStateCacheEntry &entry = g_publishedStateCache[index];
            if (entry.occupied)
            {
                continue;
            }

            entry.occupied = true;
            std::strncpy(entry.key, key, sizeof(entry.key) - 1U);
            entry.key[sizeof(entry.key) - 1U] = '\0';
            entry.payload[0] = '\0';
            return &entry;
        }

        return nullptr;
    }

    bool publishStateValue(PubSubClient &mqttClient, const String &nodeId, const char *key, const String &payload)
    {
        const String topic = getStateTopic(nodeId, key);
        return mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }

    bool publishChangedStateValue(PubSubClient &mqttClient,
                                  const String &nodeId,
                                  const char *key,
                                  const String &payload,
                                  bool forcePublish)
    {
        PublishedStateCacheEntry *entry = findPublishedStateCacheEntry(key);
        if (entry != nullptr && !forcePublish && std::strcmp(entry->payload, payload.c_str()) == 0)
        {
            return true;
        }

        if (!publishStateValue(mqttClient, nodeId, key, payload))
        {
            return false;
        }

        if (entry != nullptr)
        {
            std::strncpy(entry->payload, payload.c_str(), sizeof(entry->payload) - 1U);
            entry->payload[sizeof(entry->payload) - 1U] = '\0';
        }

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

    bool publishIntegerValue(PubSubClient &mqttClient,
                             const String &nodeId,
                             const char *key,
                             int32_t value,
                             bool forcePublish)
    {
        return publishChangedStateValue(mqttClient, nodeId, key, String(value), forcePublish);
    }

    bool publishNumberValue(PubSubClient &mqttClient,
                            const String &nodeId,
                            const HaEntityDefinition &definition,
                            int32_t rawValue,
                            bool forcePublish)
    {
        return publishChangedStateValue(mqttClient,
                                        nodeId,
                                        definition.key,
                                        formatNumberStatePayload(definition, rawValue),
                                        forcePublish);
    }

    bool publishSelectValue(PubSubClient &mqttClient,
                            const String &nodeId,
                            const HaEntityDefinition &definition,
                            int32_t rawValue,
                            bool forcePublish)
    {
        for (size_t index = 0; index < definition.optionCount; ++index)
        {
            const HaSelectOptionDefinition &option = definition.options[index];
            if (std::atoi(option.value) != rawValue)
            {
                continue;
            }

            return publishChangedStateValue(mqttClient, nodeId, definition.key, option.label, forcePublish);
        }

        return false;
    }

    bool tryGetSensorMenuValueByKey(const SensorsMenuSnapshot &snapshot, const char *key, int32_t &value)
    {
        for (uint8_t index = 1; index <= (sizeof(snapshot.values) / sizeof(snapshot.values[0])); ++index)
        {
            const SensorsMenuValueDefinition definition = getSensorsMenuValueDefinition(index);
            if (std::strcmp(definition.key, key) != 0)
            {
                continue;
            }

            const SensorsMenuValueItem &item = snapshot.values[index - 1U];
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

    bool publishCo2States(PubSubClient &mqttClient, const String &nodeId, bool forcePublish)
    {
        const Co2SensorStatus status = getCo2SensorStatus();
        if (!status.dataValid)
        {
            return true;
        }

        return publishChangedStateValue(mqttClient, nodeId, "co2_ppm", String(status.co2Ppm), forcePublish) &&
               publishChangedStateValue(mqttClient, nodeId, "co2_temperature", String(status.temperatureC, 1), forcePublish) &&
               publishChangedStateValue(mqttClient, nodeId, "co2_humidity", String(status.humidityPct, 1), forcePublish) &&
               publishChangedStateValue(mqttClient, nodeId, "co2_absolute_humidity", String(status.absoluteHumidityGm3, 1), forcePublish);
    }

    bool publishSensorMenuStates(PubSubClient &mqttClient, const String &nodeId, bool forcePublish)
    {
        const SensorsMenuSnapshot snapshot = getSensorsMenuSnapshot();
        if (snapshot.lastCompletedMs == 0)
        {
            return true;
        }

        for (size_t index = 0; index < getHaEntityDefinitionCount(); ++index)
        {
            const HaEntityDefinition *definition = getHaEntityDefinitionAt(index);
            if (definition == nullptr || definition->sourceType != HaEntitySourceType::SensorMenu)
            {
                continue;
            }

            int32_t value = 0;
            if (!tryGetSensorMenuValueByKey(snapshot, definition->key, value))
            {
                continue;
            }

            if (!publishIntegerValue(mqttClient, nodeId, definition->key, value, forcePublish))
            {
                return false;
            }
        }

        return true;
    }

    bool publishSettingsStates(PubSubClient &mqttClient, const String &nodeId, bool forcePublish)
    {
        const SettingsMenuStatus status = getSettingsMenuStatus();
        const SettingWriterStatus writerStatus = getSettingWriterStatus();
        const bool hasNewerCompletedWrite = !writerStatus.running && writerStatus.lastCompletedMs != 0 &&
                                            writerStatus.key[0] != '\0' && writerStatus.lastCompletedMs > status.lastCompletedMs;

        for (size_t index = 0; index < getHaEntityDefinitionCount(); ++index)
        {
            const HaEntityDefinition *definition = getHaEntityDefinitionAt(index);
            if (definition == nullptr || definition->sourceType != HaEntitySourceType::Setting)
            {
                continue;
            }

            if (writerStatus.running && std::strcmp(writerStatus.key, definition->key) == 0)
            {
                // Suppress stale state echoes while the writer is still walking the menu.
                continue;
            }

            int32_t rawValue = 0;
            bool hasValue = false;

            if (hasNewerCompletedWrite && std::strcmp(writerStatus.key, definition->key) == 0)
            {
                rawValue = writerStatus.value;
                hasValue = true;
            }
            else if (status.lastCompletedMs != 0)
            {
                SettingsMenuValue value{};
                if (tryGetSettingsValueByKey(status, definition->key, value) && value.hasValue)
                {
                    rawValue = value.value;
                    hasValue = true;
                }
            }

            if (!hasValue)
            {
                continue;
            }

            const bool published = definition->platform == HaEntityPlatform::Select
                                       ? publishSelectValue(mqttClient, nodeId, *definition, rawValue, forcePublish)
                                       : publishNumberValue(mqttClient, nodeId, *definition, rawValue, forcePublish);
            if (!published)
            {
                return false;
            }
        }

        return true;
    }
} // namespace

bool publishStates(PubSubClient &mqttClient, const String &nodeId, bool forcePublish)
{
    return publishCo2States(mqttClient, nodeId, forcePublish) &&
           publishSensorMenuStates(mqttClient, nodeId, forcePublish) &&
           publishSettingsStates(mqttClient, nodeId, forcePublish) &&
           publishStateValue(mqttClient, nodeId, "rssi", String(WiFi.RSSI())) &&
           publishChangedStateValue(mqttClient, nodeId, "ventilation_mode", ventilationModeStateGetLabel(), forcePublish);
}