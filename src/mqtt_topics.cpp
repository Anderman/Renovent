#include "mqtt_topics.h"
#include "ha_entity_definitions.h"

String getRootTopic(const String &nodeId)
{
    return String("renovent/") + nodeId;
}

String getAvailabilityTopic(const String &nodeId)
{
    const HaRootDefinition &root = getHaRootDefinition();
    return getRootTopic(nodeId) + "/" + root.availabilityTopicSuffix;
}

String getStateTopic(const String &nodeId, const char *key)
{
    const HaRootDefinition &root = getHaRootDefinition();
    return getRootTopic(nodeId) + "/" + root.stateTopicRoot + "/" + key;
}

String getCommandTopic(const String &nodeId, const char *key)
{
    const HaRootDefinition &root = getHaRootDefinition();
    return getRootTopic(nodeId) + "/" + root.commandTopicRoot + "/" + key;
}