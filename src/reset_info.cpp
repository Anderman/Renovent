#include "reset_info.h"

#include <esp_attr.h>
#include <esp_system.h>

namespace {
RTC_DATA_ATTR uint32_t g_bootCount = 0;

ResetInfoStatus g_status = {
    .bootCount = 0,
    .rawReason = 0,
    .reason = "unknown",
    .detail = "reset reason unavailable",
    .crashLikely = false,
};

const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:
      return "unknown";
    case ESP_RST_POWERON:
      return "poweron";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "int-wdt";
    case ESP_RST_TASK_WDT:
      return "task-wdt";
    case ESP_RST_WDT:
      return "wdt";
    case ESP_RST_DEEPSLEEP:
      return "deepsleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
    case ESP_RST_USB:
      return "usb";
    case ESP_RST_JTAG:
      return "jtag";
    case ESP_RST_EFUSE:
      return "efuse";
    case ESP_RST_PWR_GLITCH:
      return "power-glitch";
    case ESP_RST_CPU_LOCKUP:
      return "cpu-lockup";
    default:
      return "other";
  }
}

const char *resetReasonDetail(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "normale power-on of volledige spanningsonderbreking";
    case ESP_RST_SW:
      return "software reset via esp_restart of systeemsoftware";
    case ESP_RST_PANIC:
      return "CPU panic of exception; seriele panic-uitvoer bevat de backtrace";
    case ESP_RST_INT_WDT:
      return "interrupt watchdog trigger; vaak vastgelopen ISR of interrupts te lang geblokkeerd";
    case ESP_RST_TASK_WDT:
      return "task watchdog trigger; een taak of loop blokkeerde te lang";
    case ESP_RST_WDT:
      return "andere watchdog reset";
    case ESP_RST_BROWNOUT:
      return "voedingsdip of brownout detector trigger";
    case ESP_RST_PWR_GLITCH:
      return "korte voedingsonderbreking of power glitch";
    case ESP_RST_CPU_LOCKUP:
      return "CPU lockup gedetecteerd";
    case ESP_RST_EXT:
      return "externe resetlijn geactiveerd";
    case ESP_RST_DEEPSLEEP:
      return "wakeup uit deep sleep";
    default:
      return "geen extra detail beschikbaar";
  }
}

bool resetLooksCrashRelated(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_CPU_LOCKUP:
      return true;
    default:
      return false;
  }
}
}  // namespace

void resetInfoSetup() {
  const esp_reset_reason_t reason = esp_reset_reason();
  ++g_bootCount;

  g_status.bootCount = g_bootCount;
  g_status.rawReason = static_cast<uint32_t>(reason);
  g_status.reason = resetReasonName(reason);
  g_status.detail = resetReasonDetail(reason);
  g_status.crashLikely = resetLooksCrashRelated(reason);
}

void resetInfoPrintToSerial() {
  Serial.printf("[renovent] boot count: %lu\n", static_cast<unsigned long>(g_status.bootCount));
  Serial.printf("[renovent] last reset: %s (%lu)\n", g_status.reason,
                static_cast<unsigned long>(g_status.rawReason));
  Serial.printf("[renovent] reset detail: %s\n", g_status.detail);
  if (g_status.crashLikely) {
    Serial.println("[renovent] crash hint: inspect the serial panic output for a backtrace");
  }
}

ResetInfoStatus getResetInfoStatus() {
  return g_status;
}