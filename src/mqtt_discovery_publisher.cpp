#include "mqtt_discovery_publisher.h"

#include <PubSubClient.h>

#include "ha_discovery_builder.h"
#include "ha_entity_definitions.h"
#include "ota/auto_update.h"

namespace
{
    bool publishDiscoveryPayloads(PubSubClient &mqttClient,
                                  const String &nodeId,
                                  const String &availabilityTopic,
                                  const char *availabilityPayloadOnline)
    {
        const String firmwareBuildId = getCurrentFirmwareBuildId();

        if (!mqttClient.publish(availabilityTopic.c_str(), availabilityPayloadOnline, true))
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

            HaDiscoveryConfigMessage message;
            if (!buildHaDiscoveryConfigMessage(message, nodeId, availabilityTopic, firmwareBuildId, *definition))
            {
                return false;
            }

            if (!mqttClient.publish(message.topic.c_str(), message.payload.c_str(), message.retain))
            {
                return false;
            }
        }

        return true;
    }
    } // namespace

    bool mqttDiscoveryPublisherPublish(PubSubClient &mqttClient,
                                       const String &nodeId,
                                       const String &availabilityTopic,
                                       const char *availabilityPayloadOnline)
    {
        return publishDiscoveryPayloads(mqttClient, nodeId, availabilityTopic, availabilityPayloadOnline);
    }