#pragma once

#include <cstdint>
#include <stddef.h>

enum class HaEntityPlatform {
  Sensor,
  Number,
  Select,
};

enum class HaEntitySourceType {
  SensorMenu,
  Setting,
  Status,
  Co2Status,
  Virtual,
};

struct HaSelectOptionDefinition {
  const char *value;
  const char *label;
};

struct HaRootDefinition {
  const char *deviceName;
  const char *deviceModel;
  const char *deviceManufacturer;
  const char *originName;
  const char *originSupportUrl;
  const char *availabilityTopicSuffix;
  const char *stateTopicRoot;
  const char *commandTopicRoot;
};

struct HaEntityDefinition {
  const char *key;
  HaEntityPlatform platform;
  HaEntitySourceType sourceType;
  const char *title;
  const char *objectId;
  const char *uniqueIdSuffix;
  const char *defaultEntityId;
  bool enabledByDefault;
  bool writable;
  const char *entityCategory;
  const char *deviceClass;
  const char *stateClass;
  const char *unitOfMeasurement;
  int8_t suggestedDisplayPrecision;
  const HaSelectOptionDefinition *options;
  size_t optionCount;
  bool hasMin;
  float minValue;
  bool hasMax;
  float maxValue;
  bool hasStep;
  float stepValue;
  const char *numberMode;
};

const HaRootDefinition &getHaRootDefinition();
size_t getHaEntityDefinitionCount();
const HaEntityDefinition *getHaEntityDefinitionAt(size_t index);
const HaEntityDefinition *findHaEntityDefinitionByKey(const char *key);