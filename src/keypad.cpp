#include "keypad.h"

#include "keypad_internal.h"

#include "pins.h"

namespace {
struct KeyNameEntry {
  uint8_t mask;
  const char *name;
};

portMUX_TYPE g_keyMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t g_workingActiveKeys = kKeyNone;
uint8_t g_snapshotActiveKeys = kKeyNone;

constexpr KeyNameEntry kKeyNameEntries[] = {
  {kKeyOk, "ok"},
  {kKeyPlus, "plus"},
  {kKeyFunction, "function"},
  {kKeyMinus, "minus"},
};

uint8_t keyMaskForSelectIndex(uint8_t selectIndex) {
  switch (selectIndex) {
    case 1:
      return kKeyOk;
    case 3:
      return kKeyMinus;
    case 5:
      return kKeyPlus;
    case 6:
      return kKeyFunction;
    default:
      return kKeyNone;
  }
}
}  // namespace

void keypadSetup() {
  pinMode(pins::kKeyNode, INPUT);
}

void keypadPublishActiveKeysHook(uint8_t activeKeys) {
  portENTER_CRITICAL(&g_keyMux);
  g_snapshotActiveKeys = activeKeys;
  g_workingActiveKeys = kKeyNone;
  portEXIT_CRITICAL(&g_keyMux);
}

uint8_t keypadGetActiveKeys() {
  uint8_t activeKeys = kKeyNone;

  portENTER_CRITICAL(&g_keyMux);
  activeKeys = g_snapshotActiveKeys;
  portEXIT_CRITICAL(&g_keyMux);

  return activeKeys;
}

String activeKeysToString(uint8_t activeKeys) {
  if (activeKeys == kKeyNone) {
    return "none";
  }

  String result;

  for (const KeyNameEntry &entry : kKeyNameEntries) {
    if ((activeKeys & entry.mask) == 0U) {
      continue;
    }
    if (!result.isEmpty()) {
      result += "+";
    }
    result += entry.name;
  }

  return result;
}