#pragma once

#include <Arduino.h>

#include "ha_entity_definitions.h"

bool buildHaDiscoveryPayload(String &payload,
                             const String &nodeId,
                             const String &availabilityTopic,
                             const String &firmwareBuildId,
                             const HaEntityDefinition &definition);