#include "ota.h"

#include <ArduinoOTA.h>

#include "ota_config.h"

void setupOta() {
  ArduinoOTA.setHostname(otaConfig::kHostname);
  ArduinoOTA.setPassword(otaConfig::kPassword);
  ArduinoOTA.begin();
}

void otaLoop() {
  ArduinoOTA.handle();
}