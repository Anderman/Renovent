#include "mqtt_state_publisher.h"

#include <PubSubClient.h>
#include <WiFi.h>

#include "co2_sensor.h"
#include "ha_entity_definitions.h"
#include "mqtt_topics.h"
#include "sensors_menu.h"
#include "settings_menu.h"
#include "ventilation_mode_state.h"

#include <cstdlib>
#include <cstring>

namespace
{
    bool publishStateValue(PubSubClient &mqttClient, const String &nodeId, const char *key, const String &payload)
    {
        const String topic = mqttBuildStateTopic(nodeId, key);
        return mqttClient.publish(topic.c_str(), payload.c_str(), true);
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

    bool publishIntegerValue(PubSubClient &mqttClient, const String &nodeId, const char *key, int32_t value)
    {
        return publishStateValue(mqttClient, nodeId, key, String(value));
    }

    bool publishNumberValue(PubSubClient &mqttClient,
                            const String &nodeId,
                            const HaEntityDefinition &definition,
                            int32_t rawValue)
    {
        return publishStateValue(mqttClient, nodeId, definition.key, formatNumberStatePayload(definition, rawValue));
    }

    bool publishSelectValue(PubSubClient &mqttClient,
                            const String &nodeId,
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

            return publishStateValue(mqttClient, nodeId, definition.key, option.label);
        }

        return false;
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

    bool publishCo2States(PubSubClient &mqttClient, const String &nodeId)
    {
        const Co2SensorStatus status = getCo2SensorStatus();
        if (!status.dataValid)
        {
            return true;
        }

        return publishStateValue(mqttClient, nodeId, "co2_ppm", String(status.co2Ppm)) &&
               publishStateValue(mqttClient, nodeId, "co2_temperature", String(status.temperatureC, 1)) &&
               publishStateValue(mqttClient, nodeId, "co2_humidity", String(status.humidityPct, 1)) &&
               publishStateValue(mqttClient, nodeId, "co2_absolute_humidity", String(status.absoluteHumidityGm3, 1));
    }

    bool publishSensorMenuStates(PubSubClient &mqttClient, const String &nodeId)
    {
        const SensorsMenuStatus status = getSensorsMenuStatus();
        if (status.lastCompletedMs == 0)
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
            if (!tryGetSensorMenuValueByKey(status, definition->key, value))
            {
                continue;
            }

            if (!publishIntegerValue(mqttClient, nodeId, definition->key, value))
            {
                return false;
            }
        }

        return true;
    }

    bool publishSettingsStates(PubSubClient &mqttClient, const String &nodeId)
    {
        const SettingsMenuStatus status = getSettingsMenuStatus();
        if (status.lastCompletedMs == 0)
        {
            return true;
        }

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
                                       ? publishSelectValue(mqttClient, nodeId, *definition, value.value)
                                       : publishNumberValue(mqttClient, nodeId, *definition, value.value);
            if (!published)
            {
                return false;
            }
        }

        return true;
    }
} // namespace

bool mqttStatePublisherPublishAllStates(PubSubClient &mqttClient,
                                        const String &nodeId)
{
    return publishCo2States(mqttClient, nodeId) &&
           publishSensorMenuStates(mqttClient, nodeId) &&
           publishSettingsStates(mqttClient, nodeId) &&
           publishIntegerValue(mqttClient, nodeId, "rssi", WiFi.RSSI()) &&
            publishStateValue(mqttClient, nodeId, "ventilation_mode", ventilationModeStateGetLabel());
}