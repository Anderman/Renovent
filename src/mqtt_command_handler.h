#pragma once

#include <cstdint>

#include "ha_entity_definitions.h"

struct MqttQueuedCommand {
  bool occupied = false;
  char key[24] = {0};
  char payload[32] = {0};
};

void mqttCommandHandlerQueueClear();
bool mqttCommandHandlerEnqueueLatest(const char *key, const char *payload);
const MqttQueuedCommand *mqttCommandHandlerPeek();
void mqttCommandHandlerPop();
bool mqttCommandHandlerTryGetOptionValueForLabel(const HaEntityDefinition &definition,
                                                 const char *label,
                                                 int32_t &value);
bool tryParseNumber(const HaEntityDefinition &definition,
                                          const char *payload,
                                          int32_t &value);