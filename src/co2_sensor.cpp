#include "co2_sensor.h"

#include <cmath>

#include <Wire.h>

#include "pins.h"

namespace {
constexpr uint8_t kScd41Address = 0x62;
constexpr uint16_t kCommandSetAutomaticSelfCalibration = 0x2416;
constexpr uint16_t kCommandStopPeriodicMeasurement = 0x3F86;
constexpr uint16_t kCommandStartPeriodicMeasurement = 0x21B1;
constexpr uint16_t kCommandReadMeasurement = 0xEC05;
constexpr uint16_t kCommandGetDataReadyStatus = 0xE4B8;
constexpr uint32_t kPowerUpSettleMs = 1500;
constexpr uint32_t kInitRetryIntervalMs = 5000;
constexpr uint32_t kPollIntervalMs = 1000;
constexpr uint32_t kStopMeasurementSettleMs = 500;

TwoWire &g_wire = Wire;

bool g_connected = false;
bool g_measuring = false;
bool g_dataValid = false;
uint16_t g_co2Ppm = 0;
float g_temperatureC = 0.0f;
float g_humidityPct = 0.0f;
float g_absoluteHumidityGm3 = 0.0f;
uint32_t g_lastSampleMs = 0;
uint32_t g_nextInitAttemptMs = 0;
uint32_t g_lastPollMs = 0;
const char *g_error = "not initialized";

uint8_t crc8(const uint8_t *data, size_t length) {
  uint8_t crc = 0xFF;

  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80U) != 0U ? static_cast<uint8_t>((crc << 1U) ^ 0x31U)
                                : static_cast<uint8_t>(crc << 1U);
    }
  }

  return crc;
}

bool writeCommand(uint16_t command) {
  g_wire.beginTransmission(kScd41Address);
  g_wire.write(static_cast<uint8_t>(command >> 8));
  g_wire.write(static_cast<uint8_t>(command & 0xFF));
  return g_wire.endTransmission() == 0;
}

bool probeSensor() {
  g_wire.beginTransmission(kScd41Address);
  return g_wire.endTransmission() == 0;
}

bool writeCommandWithWord(uint16_t command, uint16_t value) {
  const uint8_t payload[2] = {
      static_cast<uint8_t>(value >> 8),
      static_cast<uint8_t>(value & 0xFF),
  };

  g_wire.beginTransmission(kScd41Address);
  g_wire.write(static_cast<uint8_t>(command >> 8));
  g_wire.write(static_cast<uint8_t>(command & 0xFF));
  g_wire.write(payload[0]);
  g_wire.write(payload[1]);
  g_wire.write(crc8(payload, 2));
  return g_wire.endTransmission() == 0;
}

bool readWords(uint16_t command, uint16_t *words, size_t wordCount) {
  if (!writeCommand(command)) {
    return false;
  }

  delay(1);

  const size_t byteCount = wordCount * 3U;
  if (g_wire.requestFrom(static_cast<int>(kScd41Address), static_cast<int>(byteCount)) != static_cast<int>(byteCount)) {
    return false;
  }

  for (size_t index = 0; index < wordCount; ++index) {
    uint8_t bytes[2];
    bytes[0] = g_wire.read();
    bytes[1] = g_wire.read();
    const uint8_t receivedCrc = g_wire.read();
    if (crc8(bytes, 2) != receivedCrc) {
      return false;
    }
    words[index] = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
  }

  return true;
}

void setDisconnected(const char *error) {
  g_connected = false;
  g_measuring = false;
  g_error = error;
  g_nextInitAttemptMs = millis() + kInitRetryIntervalMs;
}

bool disableSelfCalibration() {
  // ASC is a volatile setting unless persist_settings is sent. Reapplying it
  // before measurement start keeps EEPROM untouched and also covers sensor
  // re-initialization after brownouts or I2C recovery.
  if (!writeCommandWithWord(kCommandSetAutomaticSelfCalibration, 0)) {
    setDisconnected("disable ASC failed");
    return false;
  }

  return true; 
}

bool resumeMeasurementIfRunning() {
  uint16_t readyWord = 0;
  if (!readWords(kCommandGetDataReadyStatus, &readyWord, 1)) {
    return false;
  }

  g_connected = true;
  g_measuring = true;
  g_error = (readyWord & 0x07FFU) != 0U ? "resumed" : "waiting for sample";
  g_lastPollMs = 0;
  return true;
}

bool startMeasurement() {
  if (!probeSensor()) {
    setDisconnected("sensor probe failed");
    return false;
  }

  if (resumeMeasurementIfRunning()) {
    return true;
  }

  // The SCD41 can still be in periodic mode after an ESP32-only reset.
  // Bring it back to idle before sending configuration commands.
  if (writeCommand(kCommandStopPeriodicMeasurement)) {
    delay(kStopMeasurementSettleMs);
  }

  if (!disableSelfCalibration()) {
    return false;
  }

  if (!writeCommand(kCommandStartPeriodicMeasurement)) {
    setDisconnected("start measurement failed");
    return false;
  }

  g_connected = true;
  g_measuring = true;
  g_error = "warming up";
  g_lastPollMs = 0;
  return true;
}

bool isDataReady() {
  uint16_t readyWord = 0;
  if (!readWords(kCommandGetDataReadyStatus, &readyWord, 1)) {
    setDisconnected("ready check failed");
    return false;
  }

  return (readyWord & 0x07FFU) != 0U;
}

bool readMeasurement() {
  uint16_t words[3] = {0, 0, 0};
  if (!readWords(kCommandReadMeasurement, words, 3)) {
    setDisconnected("measurement read failed");
    return false;
  }

  g_dataValid = true;
  g_co2Ppm = words[0];
  g_temperatureC = -45.0f + (175.0f * static_cast<float>(words[1]) / 65536.0f);
  g_humidityPct = 100.0f * static_cast<float>(words[2]) / 65536.0f;
  const float saturationVaporPressureHpa = 6.112f * std::exp((17.67f * g_temperatureC) / (g_temperatureC + 243.5f));
  const float vaporPressureHpa = saturationVaporPressureHpa * (g_humidityPct / 100.0f);
  g_absoluteHumidityGm3 = 216.7f * (vaporPressureHpa / (g_temperatureC + 273.15f));
  g_lastSampleMs = millis();
  g_error = "ok";
  return true;
}
}

void co2SensorSetup() {
  g_wire.begin(pins::kI2cSda, pins::kI2cScl, 100000U);
  g_nextInitAttemptMs = millis() + kPowerUpSettleMs;
  g_error = "power-up settle";
}

void co2SensorLoop() {
  const uint32_t now = millis();

  if (!g_measuring) {
    if (now < g_nextInitAttemptMs) {
      return;
    }

    startMeasurement();
    return;
  }

  if (now - g_lastPollMs < kPollIntervalMs) {
    return;
  }

  g_lastPollMs = now;

  if (!isDataReady()) {
    return;
  }

  readMeasurement();
}

Co2SensorStatus getCo2SensorStatus() {
  return {
      .connected = g_connected,
      .measuring = g_measuring,
      .dataValid = g_dataValid,
      .co2Ppm = g_co2Ppm,
      .temperatureC = g_temperatureC,
      .humidityPct = g_humidityPct,
      .absoluteHumidityGm3 = g_absoluteHumidityGm3,
      .lastSampleMs = g_lastSampleMs,
      .error = g_error,
  };
}