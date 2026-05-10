#include <Arduino.h>
#include <WiFi.h>

#include "hardware/co2_sensor.h"
#include "display/display_reader.h"
#include "menu/sensors_menu.h"
#include "input/key_writer.h"
#include "input/keypad.h"
#include "mqtt/mqtt_config.h"
#include "mqtt/mqtt_runtime.h"
#include "ota/ota.h"
#include "ota/auto_update.h"
#include "core/reset_info.h"
#include "menu/setting_writer.h"
#include "hardware/status_led.h"
#include "menu/settings_menu.h"
#include "webui/webui.h"
#include "core/wifiSetup.h"

void setup()
{
  Serial.begin(115200);
  delay(1000);
  resetInfoSetup();
  mqttConfigSetup();
  mqttRuntimeSetup();
  Serial.println();
  Serial.println("[renovent] boot");
  resetInfoPrintToSerial();

  statusLedSetup();
  statusLedSetRgb(64, 0, 0);
  setupWifi();
  setupOta();
  setupAutoUpdate();
  statusLedSetRgb(0, 64, 0);

  displayReaderSetup();
  co2SensorSetup();
  keyWriterSetup();
  sensorsMenuSetup();
  settingWriterSetup();
  settingsMenuSetup();
  setupWebUi();
  setupWifiConfigPage();

  Serial.print("[renovent] webui: http://");
  Serial.println(WiFi.localIP());
  Serial.println("[renovent] wifi portal: http://<ip>:8080");
}

void loop()
{
  displayReaderLoop();
  co2SensorLoop();
  keyWriterLoop();
  sensorsMenuLoop();
  settingWriterLoop();
  settingsMenuLoop();
  otaLoop();
  autoUpdateLoop();
  webUiLoop();
  mqttRuntimeLoop();
  wifiManagerLoop();
}