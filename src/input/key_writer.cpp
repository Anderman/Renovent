#include "input/key_writer.h"

#include "input/key_writer_log.h"
#include "input/key_writer_internal.h"

#include <soc/gpio_struct.h>

#include "../hardware/pins.h"

namespace
{
  constexpr uint8_t kUpperBankBasePin = 32;
  constexpr uint32_t kKeyDownMask = 1UL << static_cast<uint32_t>(pins::kKeyDown - kUpperBankBasePin);

  portMUX_TYPE g_keyWriterMux = portMUX_INITIALIZER_UNLOCKED;
  volatile uint8_t g_injectedKeys = kKeyNone;
  uint8_t g_loggedActiveMask = kKeyNone;
  uint32_t g_autoReleaseDeadlineMs = 0;
  uint32_t g_cycleStartedMs = 0;
  uint32_t g_releaseStartedMs = 0;

  uint8_t getKeyMask(uint8_t selectIndex)
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
    const uint8_t selectKeyMask = getKeyMask(selectIndex);
    return selectKeyMask != kKeyNone && (injectedKeys & selectKeyMask) != 0U;
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
    keyWriterLogRelease(g_loggedActiveMask, relativeMsFor(now));
    g_loggedActiveMask = kKeyNone;
    g_releaseStartedMs = now;
  }

  if (activeKeys != kKeyNone)
  {
    const uint32_t idleBeforeMs = g_releaseStartedMs == 0 ? 0 : static_cast<uint32_t>(now - g_releaseStartedMs);
    g_cycleStartedMs = now;
    g_loggedActiveMask = activeKeys;
    g_releaseStartedMs = 0;
    keyWriterLogPress(activeKeys, idleBeforeMs);
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

void keyWriterApplySelectIndexHook(uint8_t selectIndex)
{
  const bool active = isKeyPhaseActive(g_injectedKeys, selectIndex);
  if (active)
  {
    GPIO.out1_w1ts.val = kKeyDownMask;
    return;
  }

  GPIO.out1_w1tc.val = kKeyDownMask;
}

void keyWriterOnDisplayChangedHook(const char *displayText)
{
  const uint32_t now = millis();

  portENTER_CRITICAL(&g_keyWriterMux);
  keyWriterLogDisplayChanged(g_loggedActiveMask,
                             relativeMsFor(now),
                             currentReleaseForMs(now),
                             displayText);
  portEXIT_CRITICAL(&g_keyWriterMux);
}

uint16_t copyKeyPressLogEntries(KeyPressLogEntry *entries, uint16_t maxEntries)
{
  if (entries == nullptr || maxEntries == 0)
  {
    return 0;
  }

  uint16_t copiedCount = 0;
  portENTER_CRITICAL(&g_keyWriterMux);
  copiedCount = keyWriterLogCopyEntries(entries, maxEntries);
  portEXIT_CRITICAL(&g_keyWriterMux);

  return copiedCount;
}