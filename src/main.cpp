#include <Arduino.h>
#include <WiFi.h>

#include "co2_sensor.h"
#include "display_reader.h"
#include "sensors_menu.h"
#include "key_writer.h"
#include "keypad.h"
#include "mqtt_config.h"
#include "ota.h"
#include "ota/auto_update.h"
#include "reset_info.h"
#include "setting_writer.h"
#include "status_led.h"
#include "settings_menu.h"
#include "webui/webui.h"
#include "wifiSetup.h"

void setup()
{
  Serial.begin(115200);
  delay(1000);
  resetInfoSetup();
  mqttConfigSetup();
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
  wifiManagerLoop();
}