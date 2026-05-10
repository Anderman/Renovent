#pragma once

#include <Arduino.h>

class KeyValueStore {
public:
  bool getBool(const char *key, bool defaultValue = false) const;
  uint8_t getUChar(const char *key, uint8_t defaultValue = 0) const;
  uint16_t getUShort(const char *key, uint16_t defaultValue = 0) const;
  uint32_t getUInt(const char *key, uint32_t defaultValue = 0) const;
  String getString(const char *key, const char *defaultValue = "") const;

  bool putBool(const char *key, bool value) const;
  bool putUChar(const char *key, uint8_t value) const;
  bool putUShort(const char *key, uint16_t value) const;
  bool putUInt(const char *key, uint32_t value) const;
  bool putString(const char *key, const String &value) const;
  bool remove(const char *key) const;
};
