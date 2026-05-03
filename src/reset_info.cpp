#include "reset_info.h"

#include <inttypes.h>

#include <esp_attr.h>
#include <esp_core_dump.h>
#include <esp_err.h>
#include <esp_system.h>

namespace {
RTC_DATA_ATTR uint32_t g_bootCount = 0;
char g_coreDumpReason[160] = {0};
char g_coreDumpBacktrace[256] = {0};

ResetInfoStatus g_status = {
  .bootCount = 0,
  .rawReason = 0,
  .reason = "unknown",
  .detail = "reset reason unavailable",
  .crashLikely = false,
  .coreDumpPresent = false,
  .coreDumpSize = 0,
  .coreDumpState = "not-checked",
  .coreDumpReason = "",
  .coreDumpBacktrace = "",
  .coreDumpBacktraceCorrupted = false,
};

void formatCoreDumpBacktrace(const esp_core_dump_bt_info_t &btInfo) {
  g_coreDumpBacktrace[0] = '\0';

  size_t used = 0;
  for (uint32_t index = 0; index < btInfo.depth && index < 16; ++index) {
    const unsigned long pc = static_cast<unsigned long>(btInfo.bt[index]);
    if (pc == 0) {
      break;
    }

    const int written = snprintf(
        g_coreDumpBacktrace + used,
        sizeof(g_coreDumpBacktrace) - used,
        "%s0x%08lX",
        used == 0 ? "" : " -> ",
        pc);
    if (written <= 0 || static_cast<size_t>(written) >= (sizeof(g_coreDumpBacktrace) - used)) {
      break;
    }
    used += static_cast<size_t>(written);
  }
}

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

const char *coreDumpStateName(esp_err_t err) {
  switch (err) {
    case ESP_OK:
      return "present";
    case ESP_ERR_NOT_FOUND:
      return "not-found";
    case ESP_ERR_INVALID_SIZE:
      return "invalid-size";
    case ESP_ERR_INVALID_CRC:
      return "invalid-crc";
    default:
      return "error";
  }
}

void updateCoreDumpStatus() {
  g_coreDumpReason[0] = '\0';
  g_coreDumpBacktrace[0] = '\0';
  g_status.coreDumpPresent = false;
  g_status.coreDumpSize = 0;
  g_status.coreDumpReason = "";
  g_status.coreDumpBacktrace = "";
  g_status.coreDumpBacktraceCorrupted = false;

  size_t coreDumpAddress = 0;
  size_t coreDumpSize = 0;
  const esp_err_t getErr = esp_core_dump_image_get(&coreDumpAddress, &coreDumpSize);
  if (getErr != ESP_OK || coreDumpSize == 0) {
    g_status.coreDumpState = coreDumpStateName(getErr);
    return;
  }

  const esp_err_t checkErr = esp_core_dump_image_check();
  g_status.coreDumpPresent = true;
  g_status.coreDumpSize = static_cast<uint32_t>(coreDumpSize);
  g_status.coreDumpState = coreDumpStateName(checkErr);

#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
  if (esp_core_dump_get_panic_reason(g_coreDumpReason, sizeof(g_coreDumpReason)) == ESP_OK) {
    g_status.coreDumpReason = g_coreDumpReason;
  }

  esp_core_dump_summary_t summary{};
  if (esp_core_dump_get_summary(&summary) == ESP_OK) {
    formatCoreDumpBacktrace(summary.exc_bt_info);
    if (g_coreDumpBacktrace[0] != '\0') {
      g_status.coreDumpBacktrace = g_coreDumpBacktrace;
      g_status.coreDumpBacktraceCorrupted = summary.exc_bt_info.corrupted;
    }
  }
#endif
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
  updateCoreDumpStatus();
}

void resetInfoPrintToSerial() {
  Serial.printf("[renovent] boot count: %lu\n", static_cast<unsigned long>(g_status.bootCount));
  Serial.printf("[renovent] last reset: %s (%lu)\n", g_status.reason,
                static_cast<unsigned long>(g_status.rawReason));
  Serial.printf("[renovent] reset detail: %s\n", g_status.detail);
  Serial.printf("[renovent] coredump: %s, present=%s, size=%lu\n",
                g_status.coreDumpState,
                g_status.coreDumpPresent ? "yes" : "no",
                static_cast<unsigned long>(g_status.coreDumpSize));
  if (g_status.coreDumpReason != nullptr && g_status.coreDumpReason[0] != '\0') {
    Serial.printf("[renovent] coredump reason: %s\n", g_status.coreDumpReason);
  }
  if (g_status.crashLikely) {
    Serial.println("[renovent] crash hint: inspect the serial panic output for a backtrace");
  }
}

ResetInfoStatus getResetInfoStatus() {
  return g_status;
}