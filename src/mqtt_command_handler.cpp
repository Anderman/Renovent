#include "mqtt_command_handler.h"

#include <Arduino.h>

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>

namespace {

constexpr size_t kCommandQueueCapacity = 12;

MqttQueuedCommand g_commandQueue[kCommandQueueCapacity];
size_t g_commandQueueHead = 0;
size_t g_commandQueueCount = 0;

bool queueIsFull() {
  return g_commandQueueCount >= kCommandQueueCapacity;
}

bool queueIsEmpty() {
  return g_commandQueueCount == 0;
}

void rebuildQueueWithoutKey(const char *key) {
  if (key == nullptr || queueIsEmpty()) {
    return;
  }

  MqttQueuedCommand filteredQueue[kCommandQueueCapacity] = {};
  size_t filteredCount = 0;

  for (size_t offset = 0; offset < g_commandQueueCount; ++offset) {
    const size_t index = (g_commandQueueHead + offset) % kCommandQueueCapacity;
    const MqttQueuedCommand &entry = g_commandQueue[index];
    if (!entry.occupied || std::strcmp(entry.key, key) == 0) {
      continue;
    }

    filteredQueue[filteredCount++] = entry;
  }

  mqttCommandHandlerQueueClear();
  for (size_t index = 0; index < filteredCount; ++index) {
    g_commandQueue[index] = filteredQueue[index];
  }
  g_commandQueueCount = filteredCount;
}

bool parseIntegerPayload(const char *payload, int32_t &value) {
  if (payload == nullptr || payload[0] == '\0') {
    return false;
  }

  char *end = nullptr;
  const long parsed = std::strtol(payload, &end, 10);
  if (end == payload) {
    return false;
  }

  while (end != nullptr && *end != '\0') {
    if (!std::isspace(static_cast<unsigned char>(*end))) {
      return false;
    }
    ++end;
  }

  value = static_cast<int32_t>(parsed);
  return true;
}

int32_t numberScaleFactor(const HaEntityDefinition &definition) {
  if (definition.suggestedDisplayPrecision <= 0) {
    return 1;
  }

  int32_t factor = 1;
  for (int8_t index = 0; index < definition.suggestedDisplayPrecision; ++index) {
    factor *= 10;
  }

  return factor;
}

}  // namespace

void mqttCommandHandlerQueueClear() {
  for (size_t index = 0; index < kCommandQueueCapacity; ++index) {
    g_commandQueue[index] = MqttQueuedCommand{};
  }

  g_commandQueueHead = 0;
  g_commandQueueCount = 0;
}

bool mqttCommandHandlerEnqueueLatest(const char *key, const char *payload) {
  if (key == nullptr || payload == nullptr) {
    return false;
  }

  rebuildQueueWithoutKey(key);
  if (queueIsFull()) {
    return false;
  }

  const size_t tail = (g_commandQueueHead + g_commandQueueCount) % kCommandQueueCapacity;
  MqttQueuedCommand &entry = g_commandQueue[tail];
  entry = MqttQueuedCommand{};
  entry.occupied = true;
  std::strncpy(entry.key, key, sizeof(entry.key) - 1);
  entry.key[sizeof(entry.key) - 1] = '\0';
  std::strncpy(entry.payload, payload, sizeof(entry.payload) - 1);
  entry.payload[sizeof(entry.payload) - 1] = '\0';
  ++g_commandQueueCount;
  return true;
}

const MqttQueuedCommand *mqttCommandHandlerPeek() {
  if (queueIsEmpty()) {
    return nullptr;
  }

  return &g_commandQueue[g_commandQueueHead];
}

void mqttCommandHandlerPop() {
  if (queueIsEmpty()) {
    return;
  }

  g_commandQueue[g_commandQueueHead] = MqttQueuedCommand{};
  g_commandQueueHead = (g_commandQueueHead + 1U) % kCommandQueueCapacity;
  --g_commandQueueCount;
}

bool mqttCommandHandlerIsNumericSettingKey(const char *key) {
  if (key == nullptr) {
    return false;
  }

  return key[0] == 'U' || key[0] == 'I';
}

bool mqttCommandHandlerTryGetOptionValueForLabel(const HaEntityDefinition &definition,
                                                 const char *label,
                                                 int32_t &value) {
  if (label == nullptr) {
    return false;
  }

  for (size_t index = 0; index < definition.optionCount; ++index) {
    const HaSelectOptionDefinition &option = definition.options[index];
    if (std::strcmp(option.label, label) != 0 && std::strcmp(option.value, label) != 0) {
      continue;
    }

    value = std::atoi(option.value);
    return true;
  }

  return false;
}

bool mqttCommandHandlerParseNumberPayload(const HaEntityDefinition &definition,
                                          const char *payload,
                                          int32_t &value) {
  if (payload == nullptr || payload[0] == '\0') {
    return false;
  }

  const int32_t scaleFactor = numberScaleFactor(definition);
  if (scaleFactor == 1) {
    return parseIntegerPayload(payload, value);
  }

  char *end = nullptr;
  const double parsed = std::strtod(payload, &end);
  if (end == payload) {
    return false;
  }

  while (end != nullptr && *end != '\0') {
    if (!std::isspace(static_cast<unsigned char>(*end))) {
      return false;
    }
    ++end;
  }

  value = static_cast<int32_t>(std::lround(parsed * static_cast<double>(scaleFactor)));
  return true;
}