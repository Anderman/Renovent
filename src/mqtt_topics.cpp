#include "mqtt_topics.h"

#include "ha_entity_definitions.h"

String mqttBuildRenoventRootTopic(const String &nodeId)
{
    return String("renovent/") + nodeId;
}

String mqttBuildAvailabilityTopic(const String &nodeId)
{
    const HaRootDefinition &root = getHaRootDefinition();
    return mqttBuildRenoventRootTopic(nodeId) + "/" + root.availabilityTopicSuffix;
}

String mqttBuildStateTopic(const String &nodeId, const char *key)
{
    const HaRootDefinition &root = getHaRootDefinition();
    return mqttBuildRenoventRootTopic(nodeId) + "/" + root.stateTopicRoot + "/" + key;
}

String mqttBuildCommandTopic(const String &nodeId, const char *key)
{
    const HaRootDefinition &root = getHaRootDefinition();
    return mqttBuildRenoventRootTopic(nodeId) + "/" + root.commandTopicRoot + "/" + key;
}