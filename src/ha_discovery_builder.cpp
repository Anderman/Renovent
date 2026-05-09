#include "ha_discovery_builder.h"

#include <ArduinoJson.h>
#include <cstring>

#include "mqtt_topics.h"

namespace {

const char *platformToDiscoverySegment(HaEntityPlatform platform) {
  switch (platform) {
    case HaEntityPlatform::Number:
      return "number";
    case HaEntityPlatform::Select:
      return "select";
    case HaEntityPlatform::Sensor:
      return "sensor";
  }

  return "sensor";
}

constexpr char kAvailabilityPayloadOnline[] = "online";
constexpr char kAvailabilityPayloadOffline[] = "offline";

bool shouldIncludeDeviceClass(const HaEntityDefinition &definition) {
  if (definition.deviceClass == nullptr || definition.deviceClass[0] == '\0') {
    return false;
  }

  return !(definition.platform == HaEntityPlatform::Sensor &&
           std::strcmp(definition.deviceClass, "enum") == 0 &&
           definition.optionCount == 0);
}

String buildDiscoveryTopic(const String &nodeId, const HaEntityDefinition &definition) {
  return String("homeassistant/") + platformToDiscoverySegment(definition.platform) + "/" +
         nodeId + "/" + definition.objectId + "/config";
}

void populateCommonPayloadFields(JsonDocument &doc,
                                 const String &nodeId,
                                 const String &availabilityTopic,
                                 const String &firmwareBuildId,
                                 const HaEntityDefinition &definition) {
  const HaRootDefinition &root = getHaRootDefinition();

  doc["unique_id"] = nodeId + "_" + definition.uniqueIdSuffix;
  doc["object_id"] = definition.objectId;
  doc["name"] = definition.title;
  doc["enabled_by_default"] = definition.enabledByDefault;
  doc["availability_topic"] = availabilityTopic;
  doc["payload_available"] = kAvailabilityPayloadOnline;
  doc["payload_not_available"] = kAvailabilityPayloadOffline;

  if (definition.defaultEntityId != nullptr && definition.defaultEntityId[0] != '\0') {
    doc["default_entity_id"] = definition.defaultEntityId;
  }

  if (definition.entityCategory != nullptr && definition.entityCategory[0] != '\0') {
    doc["entity_category"] = definition.entityCategory;
  }

  JsonObject device = doc["device"].to<JsonObject>();
  JsonArray identifiers = device["identifiers"].to<JsonArray>();
  identifiers.add(nodeId);
  device["name"] = root.deviceName;
  device["model"] = root.deviceModel;
  device["manufacturer"] = root.deviceManufacturer;
  if (!firmwareBuildId.isEmpty()) {
    device["sw_version"] = firmwareBuildId;
  }

  JsonObject origin = doc["origin"].to<JsonObject>();
  origin["name"] = root.originName;
  if (!firmwareBuildId.isEmpty()) {
    origin["sw_version"] = firmwareBuildId;
  }
  if (root.originSupportUrl != nullptr && root.originSupportUrl[0] != '\0') {
    origin["support_url"] = root.originSupportUrl;
  }
}

void populateSensorPayload(JsonDocument &doc,
                           const String &nodeId,
                           const HaEntityDefinition &definition) {
  doc["state_topic"] = getStateTopic(nodeId, definition.key);
  if (shouldIncludeDeviceClass(definition)) {
    doc["device_class"] = definition.deviceClass;
  }
  if (definition.stateClass != nullptr && definition.stateClass[0] != '\0') {
    doc["state_class"] = definition.stateClass;
  }
  if (definition.unitOfMeasurement != nullptr && definition.unitOfMeasurement[0] != '\0') {
    doc["unit_of_measurement"] = definition.unitOfMeasurement;
  }
  if (definition.suggestedDisplayPrecision >= 0) {
    doc["suggested_display_precision"] = definition.suggestedDisplayPrecision;
  }
}

void populateNumberPayload(JsonDocument &doc,
                           const String &nodeId,
                           const HaEntityDefinition &definition) {
  doc["state_topic"] = getStateTopic(nodeId, definition.key);
  doc["command_topic"] = getCommandTopic(nodeId, definition.key);
  doc["optimistic"] = false;

  if (definition.hasMin) {
    doc["min"] = definition.minValue;
  }
  if (definition.hasMax) {
    doc["max"] = definition.maxValue;
  }
  if (definition.hasStep) {
    doc["step"] = definition.stepValue;
  }
  if (definition.numberMode != nullptr && definition.numberMode[0] != '\0') {
    doc["mode"] = definition.numberMode;
  }
}

void populateSelectPayload(JsonDocument &doc,
                           const String &nodeId,
                           const HaEntityDefinition &definition) {
  doc["state_topic"] = getStateTopic(nodeId, definition.key);
  doc["command_topic"] = getCommandTopic(nodeId, definition.key);
  doc["optimistic"] = false;

  JsonArray options = doc["options"].to<JsonArray>();
  for (size_t index = 0; index < definition.optionCount; ++index) {
    options.add(definition.options[index].label);
  }
}

}  // namespace

bool buildHaDiscoveryConfigMessage(HaDiscoveryConfigMessage &message,
                                   const String &nodeId,
                                   const String &availabilityTopic,
                                   const String &firmwareBuildId,
                                   const HaEntityDefinition &definition) {
  JsonDocument doc;
  populateCommonPayloadFields(doc, nodeId, availabilityTopic, firmwareBuildId, definition);

  switch (definition.platform) {
    case HaEntityPlatform::Sensor:
      populateSensorPayload(doc, nodeId, definition);
      break;
    case HaEntityPlatform::Number:
      populateNumberPayload(doc, nodeId, definition);
      break;
    case HaEntityPlatform::Select:
      populateSelectPayload(doc, nodeId, definition);
      break;
  }

  message = HaDiscoveryConfigMessage{};
  message.topic = buildDiscoveryTopic(nodeId, definition);
  return serializeJson(doc, message.payload) > 0;
}