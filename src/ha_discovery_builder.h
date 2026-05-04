#pragma once

#include <Arduino.h>

#include "ha_entity_definitions.h"

struct HaDiscoveryConfigMessage
{
    String topic;
    String payload;
    bool retain = true;
};

bool buildHaDiscoveryConfigMessage(HaDiscoveryConfigMessage &message,
                                   const String &nodeId,
                                   const String &availabilityTopic,
                                   const String &firmwareBuildId,
                                   const HaEntityDefinition &definition);