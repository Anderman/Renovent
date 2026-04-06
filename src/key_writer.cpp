#include "key_writer.h"

#include <cstring>

#include <soc/gpio_struct.h>

#include "keypad.h"
#include "pins.h"

namespace
{
  constexpr uint8_t kUpperBankBasePin = 32;
  constexpr uint32_t kKeyDownMask = 1UL << static_cast<uint32_t>(pins::kKeyDown - kUpperBankBasePin);
  constexpr uint16_t kMaxLogEntries = 400;

  portMUX_TYPE g_keyWriterMux = portMUX_INITIALIZER_UNLOCKED;
  volatile uint8_t g_injectedKeys = kKeyNone;
  uint8_t g_loggedActiveMask = kKeyNone;
  uint32_t g_autoReleaseDeadlineMs = 0;
  uint32_t g_cycleStartedMs = 0;
  uint32_t g_releaseStartedMs = 0;
  KeyPressLogEntry g_logEntries[kMaxLogEntries] = {};
  uint16_t g_logNextIndex = 0;
  uint16_t g_logCount = 0;
  char g_lastDisplayText[9] = {0};

  uint8_t keyMaskForSelectIndex(uint8_t selectIndex)
  {
    switch (selectIndex)
    {
    case 1:
      return kKeyOk;
    case 3:
      return kKeyFunction;
    case 5:
      return kKeyMinus;
    case 6:
      return kKeyPlus;
    default:
      return kKeyNone;
    }
  }

  void setKeyDownIdleLow()
  {
    GPIO.out1_w1tc.val = kKeyDownMask;
  }

  bool isKeyPhaseActive(uint8_t injectedKeys, uint8_t selectIndex)
  {
    const uint8_t selectKeyMask = keyMaskForSelectIndex(selectIndex);
    return selectKeyMask != kKeyNone && (injectedKeys & selectKeyMask) != 0U;
  }

  void copyKeyText(char (&destination)[24], uint8_t mask)
  {
    const String text = activeKeysToString(mask);
    std::strncpy(destination, text.c_str(), sizeof(destination) - 1);
    destination[sizeof(destination) - 1] = '\0';
  }

  void copyDisplayText(char (&destination)[9], const char *source)
  {
    if (source == nullptr)
    {
      destination[0] = '\0';
      return;
    }

    std::strncpy(destination, source, sizeof(destination) - 1);
    destination[sizeof(destination) - 1] = '\0';
  }

  uint32_t relativeMsFor(uint32_t now)
  {
    if (g_cycleStartedMs == 0)
    {
      return 0;
    }

    return static_cast<uint32_t>(now - g_cycleStartedMs);
  }

  uint32_t currentReleaseForMs(uint32_t now)
  {
    if (g_loggedActiveMask != kKeyNone || g_releaseStartedMs == 0)
    {
      return 0;
    }

    return static_cast<uint32_t>(now - g_releaseStartedMs);
  }

  void appendLogEntry(const char *eventName,
                      uint8_t mask,
                      uint32_t relativeMs,
                      uint32_t idleBeforeMs,
                      uint32_t releaseForMs,
                      const char *displayText)
  {
    KeyPressLogEntry &entry = g_logEntries[g_logNextIndex];
    entry.available = true;
    std::strncpy(entry.event, eventName, sizeof(entry.event) - 1);
    entry.event[sizeof(entry.event) - 1] = '\0';
    entry.mask = mask;
    entry.relativeMs = relativeMs;
    entry.idleBeforeMs = idleBeforeMs;
    entry.releaseForMs = releaseForMs;
    copyKeyText(entry.keys, mask);
    copyDisplayText(entry.display, displayText);

    g_logNextIndex = static_cast<uint16_t>((g_logNextIndex + 1U) % kMaxLogEntries);
    if (g_logCount < kMaxLogEntries)
    {
      ++g_logCount;
    }
  }
} // namespace

void keyWriterSetup()
{
  pinMode(pins::kKeyDown, OUTPUT);
  setKeyDownIdleLow();
}

void keyWriterLoop()
{
  bool shouldRelease = false;

  portENTER_CRITICAL(&g_keyWriterMux);
  if (g_autoReleaseDeadlineMs != 0 &&
      static_cast<int32_t>(millis() - g_autoReleaseDeadlineMs) >= 0)
  {
    g_autoReleaseDeadlineMs = 0;
    shouldRelease = true;
  }
  portEXIT_CRITICAL(&g_keyWriterMux);

  if (shouldRelease)
  {
    pressKeys(kKeyNone);
  }
}

void pressKeys(uint8_t activeKeys)
{
  const uint32_t now = millis();

  portENTER_CRITICAL(&g_keyWriterMux);
  if (g_loggedActiveMask == activeKeys)
  {
    g_injectedKeys = activeKeys;
    if (activeKeys == kKeyNone)
    {
      g_autoReleaseDeadlineMs = 0;
    }
    portEXIT_CRITICAL(&g_keyWriterMux);
    return;
  }

  if (g_loggedActiveMask != kKeyNone)
  {
    appendLogEntry("release", g_loggedActiveMask, relativeMsFor(now), 0, 0, g_lastDisplayText);
    g_loggedActiveMask = kKeyNone;
    g_releaseStartedMs = now;
  }

  if (activeKeys != kKeyNone)
  {
    const uint32_t idleBeforeMs = g_releaseStartedMs == 0 ? 0 : static_cast<uint32_t>(now - g_releaseStartedMs);
    g_cycleStartedMs = now;
    g_loggedActiveMask = activeKeys;
    g_releaseStartedMs = 0;
    appendLogEntry("press", activeKeys, 0, idleBeforeMs, 0, g_lastDisplayText);
  }

  g_injectedKeys = activeKeys;
  g_autoReleaseDeadlineMs = 0;
  portEXIT_CRITICAL(&g_keyWriterMux);
}

void pulseKeys(uint8_t activeKeys, uint32_t holdMs)
{
  if (activeKeys == kKeyNone || holdMs == 0)
  {
    pressKeys(activeKeys);
    return;
  }

  pressKeys(activeKeys);

  portENTER_CRITICAL(&g_keyWriterMux);
  g_autoReleaseDeadlineMs = millis() + holdMs;
  portEXIT_CRITICAL(&g_keyWriterMux);
}

void keyWriterOnSelectIndex(uint8_t selectIndex)
{
  const bool active = isKeyPhaseActive(g_injectedKeys, selectIndex);
  if (active)
  {
    GPIO.out1_w1ts.val = kKeyDownMask;
    return;
  }

  GPIO.out1_w1tc.val = kKeyDownMask;
}

void keyWriterOnDisplayTextChanged(const char *displayText)
{
  const uint32_t now = millis();

  portENTER_CRITICAL(&g_keyWriterMux);
  if (displayText == nullptr || std::strncmp(g_lastDisplayText, displayText, sizeof(g_lastDisplayText)) == 0)
  {
    portEXIT_CRITICAL(&g_keyWriterMux);
    return;
  }

  copyDisplayText(g_lastDisplayText, displayText);
  appendLogEntry("display",
                 g_loggedActiveMask,
                 relativeMsFor(now),
                 0,
                 currentReleaseForMs(now),
                 g_lastDisplayText);
  portEXIT_CRITICAL(&g_keyWriterMux);
}

KeyPressLogSummary getKeyPressLogSummary()
{
  KeyPressLogSummary status{};
  const uint32_t now = millis();

  portENTER_CRITICAL(&g_keyWriterMux);
  status.activeMask = g_loggedActiveMask;
  status.activeRelativeMs = g_loggedActiveMask == kKeyNone ? 0 : relativeMsFor(now);
  status.releaseForMs = currentReleaseForMs(now);
  copyKeyText(status.activeKeys, g_loggedActiveMask);
  copyDisplayText(status.lastDisplayText, g_lastDisplayText);
  status.count = g_logCount;
  portEXIT_CRITICAL(&g_keyWriterMux);

  return status;
}

bool getKeyPressLogEntryNewestFirst(uint16_t newestFirstIndex, KeyPressLogEntry &entry)
{
  bool available = false;

  portENTER_CRITICAL(&g_keyWriterMux);
  if (newestFirstIndex < g_logCount)
  {
    const uint16_t sourceIndex = static_cast<uint16_t>((g_logNextIndex + kMaxLogEntries - 1U - newestFirstIndex) % kMaxLogEntries);
    entry = g_logEntries[sourceIndex];
    available = entry.available;
  }
  portEXIT_CRITICAL(&g_keyWriterMux);

  return available;
}