#pragma once

#include <Arduino.h>

struct Co2SensorStatus {
  bool connected;
  bool measuring;
  bool dataValid;
  uint16_t co2Ppm;
  float temperatureC;
  float humidityPct;
  float absoluteHumidityGm3;
  uint32_t lastSampleMs;
  const char *error;
};

void co2SensorSetup();
void co2SensorLoop();
Co2SensorStatus getCo2SensorStatus();