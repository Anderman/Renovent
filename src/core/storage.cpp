#include "core/storage.h"

#include <Preferences.h>

bool KeyValueStore::getBool(const char *key, bool defaultValue) const {
  Preferences prefs;
  if (!prefs.begin("_", true)) {
    return defaultValue;
  }
  const bool value = prefs.getBool(key, defaultValue);
  prefs.end();
  return value;
}

uint8_t KeyValueStore::getUChar(const char *key, uint8_t defaultValue) const {
  Preferences prefs;
  if (!prefs.begin("_", true)) {
    return defaultValue;
  }
  const uint8_t value = prefs.getUChar(key, defaultValue);
  prefs.end();
  return value;
}

uint16_t KeyValueStore::getUShort(const char *key, uint16_t defaultValue) const {
  Preferences prefs;
  if (!prefs.begin("_", true)) {
    return defaultValue;
  }
  const uint16_t value = prefs.getUShort(key, defaultValue);
  prefs.end();
  return value;
}

uint32_t KeyValueStore::getUInt(const char *key, uint32_t defaultValue) const {
  Preferences prefs;
  if (!prefs.begin("_", true)) {
    return defaultValue;
  }
  const uint32_t value = prefs.getUInt(key, defaultValue);
  prefs.end();
  return value;
}

String KeyValueStore::getString(const char *key, const char *defaultValue) const {
  Preferences prefs;
  if (!prefs.begin("_", true)) {
    return String(defaultValue);
  }
  const String value = prefs.getString(key, defaultValue);
  prefs.end();
  return value;
}

bool KeyValueStore::putBool(const char *key, bool value) const {
  Preferences prefs;
  if (!prefs.begin("_", false)) {
    return false;
  }
  const bool ok = prefs.putBool(key, value) > 0;
  prefs.end();
  return ok;
}

bool KeyValueStore::putUChar(const char *key, uint8_t value) const {
  Preferences prefs;
  if (!prefs.begin("_", false)) {
    return false;
  }
  const bool ok = prefs.putUChar(key, value) > 0;
  prefs.end();
  return ok;
}

bool KeyValueStore::putUShort(const char *key, uint16_t value) const {
  Preferences prefs;
  if (!prefs.begin("_", false)) {
    return false;
  }
  const bool ok = prefs.putUShort(key, value) > 0;
  prefs.end();
  return ok;
}

bool KeyValueStore::putUInt(const char *key, uint32_t value) const {
  Preferences prefs;
  if (!prefs.begin("_", false)) {
    return false;
  }
  const bool ok = prefs.putUInt(key, value) > 0;
  prefs.end();
  return ok;
}

bool KeyValueStore::putString(const char *key, const String &value) const {
  Preferences prefs;
  if (!prefs.begin("_", false)) {
    return false;
  }
  const bool ok = prefs.putString(key, value) > 0;
  prefs.end();
  return ok;
}

bool KeyValueStore::remove(const char *key) const {
  Preferences prefs;
  if (!prefs.begin("_", false)) {
    return false;
  }
  const bool ok = prefs.remove(key);
  prefs.end();
  return ok;
}

