#include "ha_entity_definitions.h"

#include <cstring>

namespace
{

  constexpr HaRootDefinition kHaRootDefinition = {
      "Renovent",
      "ESP32",
      "1001",
      "renovent2mqtt",
      "",
      "availability",
      "state",
      "command",
  };

  constexpr HaSelectOptionDefinition kVentilationModeOptions[] = {
      {"AAN", "AAN"},
      {"UIT", "UIT"},
      {"AUTO", "AUTO"},
  };

  constexpr HaSelectOptionDefinition kBinaryOptions[] = {
      {"0", "Uit"},
      {"1", "Aan"},
  };

  constexpr HaSelectOptionDefinition kStageOptions[] = {
      {"0", "Stand 0"},
      {"1", "Stand 1"},
      {"2", "Stand 2"},
      {"3", "Stand 3"},
  };

  constexpr HaSelectOptionDefinition kStage23Options[] = {
      {"2", "Stand 2"},
      {"3", "Stand 3"},
  };

  constexpr HaSelectOptionDefinition kBypassModeOptions[] = {
      {"0", "Nooit"},
      {"1", "Automatisch"},
      {"2", "Toevoer minimaal"},
  };

  constexpr HaSelectOptionDefinition kHeaterTypeOptions[] = {
      {"0", "Geen"},
      {"1", "Voorverwarmer"},
      {"2", "Naverwarmer"},
      {"3", "Voor- en naverwarmer"},
  };

  constexpr HaSelectOptionDefinition kWtwConfigurationOptions[] = {
      {"0", "WTW"},
      {"1", "CV + WTW"},
  };

  constexpr HaSelectOptionDefinition kFanOffOptions[] = {
      {"1", "Afvoerventilator"},
      {"2", "Toevoerventilator"},
      {"3", "Beide ventilatoren"},
  };

  constexpr HaEntityDefinition kHaEntityDefinitions[] = {
      {"co2_ppm", HaEntityPlatform::Sensor, HaEntitySourceType::Co2Status, "CO2", "co2_ppm", "co2_ppm", "sensor.renovent_co2_ppm", true, false, nullptr, "carbon_dioxide", "measurement", "ppm", 0, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"co2_temperature", HaEntityPlatform::Sensor, HaEntitySourceType::Co2Status, "CO2 temperatuur", "co2_temperature", "co2_temperature", "sensor.renovent_co2_temperature", true, false, nullptr, "temperature", "measurement", "°C", 1, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"co2_humidity", HaEntityPlatform::Sensor, HaEntitySourceType::Co2Status, "CO2 vocht", "co2_humidity", "co2_humidity", "sensor.renovent_co2_humidity", true, false, nullptr, "humidity", "measurement", "%", 1, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"co2_absolute_humidity", HaEntityPlatform::Sensor, HaEntitySourceType::Co2Status, "Absolute vochtigheid", "co2_absolute_humidity", "co2_absolute_humidity", "sensor.renovent_co2_absolute_humidity", true, false, nullptr, nullptr, "measurement", "g/m3", 1, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"fan_mode", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Actuele stand", "fan_mode", "fan_mode", "sensor.renovent_fan_mode", true, false, nullptr, "enum", nullptr, nullptr, -1, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"exhaust_setpoint", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Afvoer setpoint", "exhaust_setpoint", "exhaust_setpoint", "sensor.renovent_exhaust_setpoint", true, false, nullptr, nullptr, "measurement", "m3/h", 0, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"operation_code", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Meldcode", "operation_code", "operation_code", "sensor.renovent_operation_code", false, false, nullptr, "enum", nullptr, nullptr, -1, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"bypass_status", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Bypass status", "bypass_status", "bypass_status", "sensor.renovent_bypass_status", true, false, nullptr, "enum", nullptr, nullptr, -1, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"outside_temperature", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Temperatuur van buiten", "outside_temperature", "outside_temperature", "sensor.renovent_outside_temperature", true, false, nullptr, "temperature", "measurement", "°C", 0, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"inside_temperature", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Temperatuur van binnen", "inside_temperature", "inside_temperature", "sensor.renovent_inside_temperature", true, false, nullptr, "temperature", "measurement", "°C", 0, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"supply_flow", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Toevoer volume", "supply_flow", "supply_flow", "sensor.renovent_supply_flow", true, false, nullptr, nullptr, "measurement", "m3/h", 0, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"exhaust_flow", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Afvoer volume", "exhaust_flow", "exhaust_flow", "sensor.renovent_exhaust_flow", true, false, nullptr, nullptr, "measurement", "m3/h", 0, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"supply_pressure", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Toevoer druk", "supply_pressure", "supply_pressure", "sensor.renovent_supply_pressure", true, false, nullptr, nullptr, "measurement", "Pa", 0, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"exhaust_pressure", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Afvoer druk", "exhaust_pressure", "exhaust_pressure", "sensor.renovent_exhaust_pressure", true, false, nullptr, nullptr, "measurement", "Pa", 0, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"frost_protection", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Vorstbeveiliging", "frost_protection", "frost_protection", "sensor.renovent_frost_protection", false, false, nullptr, "enum", nullptr, nullptr, -1, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"outgoing_temperature", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Temperatuur naar buiten", "outgoing_temperature", "outgoing_temperature", "sensor.renovent_outgoing_temperature", false, false, nullptr, "temperature", "measurement", "°C", 0, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"incoming_temperature", HaEntityPlatform::Sensor, HaEntitySourceType::SensorMenu, "Temperatuur naar binnen", "incoming_temperature", "incoming_temperature", "sensor.renovent_incoming_temperature", false, false, nullptr, "temperature", "measurement", "°C", 0, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"rssi", HaEntityPlatform::Sensor, HaEntitySourceType::Status, "RSSI", "rssi", "rssi", "sensor.renovent_rssi", true, false, "diagnostic", "signal_strength", "measurement", "dBm", 0, nullptr, 0, false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"ventilation_mode", HaEntityPlatform::Select, HaEntitySourceType::Virtual, "Ventilatiemodus", "ventilation_mode", "ventilation_mode", "select.renovent_ventilation_mode", true, true, nullptr, nullptr, nullptr, nullptr, -1, kVentilationModeOptions, sizeof(kVentilationModeOptions) / sizeof(kVentilationModeOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"U1", HaEntityPlatform::Number, HaEntitySourceType::Setting, "Volume stap 1", "U1", "U1", "number.renovent_u1", true, true, nullptr, nullptr, nullptr, nullptr, -1, nullptr, 0, true, 50.0f, true, 400.0f, true, 5.0f, "box"},
      {"U2", HaEntityPlatform::Number, HaEntitySourceType::Setting, "Volume stap 2", "U2", "U2", "number.renovent_u2", true, true, nullptr, nullptr, nullptr, nullptr, -1, nullptr, 0, true, 50.0f, true, 400.0f, true, 5.0f, "box"},
      {"U3", HaEntityPlatform::Number, HaEntitySourceType::Setting, "Volume stap 3", "U3", "U3", "number.renovent_u3", true, true, nullptr, nullptr, nullptr, nullptr, -1, nullptr, 0, true, 50.0f, true, 400.0f, true, 5.0f, "box"},
      {"U4", HaEntityPlatform::Number, HaEntitySourceType::Setting, "Minimum buitentemperatuur bypass", "U4", "U4", "number.renovent_u4", true, true, nullptr, "temperature", nullptr, "°C", 0, nullptr, 0, true, 5.0f, true, 20.0f, true, 1.0f, "box"},
      {"U5", HaEntityPlatform::Number, HaEntitySourceType::Setting, "Minimum binnentemperatuur bypass", "U5", "U5", "number.renovent_u5", true, true, nullptr, "temperature", nullptr, "°C", 0, nullptr, 0, true, 18.0f, true, 30.0f, true, 1.0f, "box"},
      {"I1", HaEntityPlatform::Number, HaEntitySourceType::Setting, "Vaste onbalans", "I1", "I1", "number.renovent_i1", false, true, nullptr, nullptr, nullptr, nullptr, -1, nullptr, 0, true, -100.0f, true, 100.0f, true, 1.0f, "box"},
      {"I2", HaEntityPlatform::Select, HaEntitySourceType::Setting, "Geen contact stap", "I2", "I2", "select.renovent_i2", false, true, nullptr, nullptr, nullptr, nullptr, -1, kStageOptions, sizeof(kStageOptions) / sizeof(kStageOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I3", HaEntityPlatform::Select, HaEntitySourceType::Setting, "Perilex L2 stap", "I3", "I3", "select.renovent_i3", false, true, nullptr, nullptr, nullptr, nullptr, -1, kStage23Options, sizeof(kStage23Options) / sizeof(kStage23Options[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I4", HaEntityPlatform::Select, HaEntitySourceType::Setting, "Switch lijn 1 stap", "I4", "I4", "select.renovent_i4", false, true, nullptr, nullptr, nullptr, nullptr, -1, kStageOptions, sizeof(kStageOptions) / sizeof(kStageOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I5", HaEntityPlatform::Select, HaEntitySourceType::Setting, "Switch lijn 2 stap", "I5", "I5", "select.renovent_i5", false, true, nullptr, nullptr, nullptr, nullptr, -1, kStageOptions, sizeof(kStageOptions) / sizeof(kStageOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I6", HaEntityPlatform::Select, HaEntitySourceType::Setting, "Switch lijn 3 stap", "I6", "I6", "select.renovent_i6", false, true, nullptr, nullptr, nullptr, nullptr, -1, kStageOptions, sizeof(kStageOptions) / sizeof(kStageOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I7", HaEntityPlatform::Select, HaEntitySourceType::Setting, "Onbalans toelaatbaar", "I7", "I7", "select.renovent_i7", false, true, nullptr, nullptr, nullptr, nullptr, -1, kBinaryOptions, sizeof(kBinaryOptions) / sizeof(kBinaryOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I8", HaEntityPlatform::Select, HaEntitySourceType::Setting, "Bypass-modus", "I8", "I8", "select.renovent_i8", true, true, nullptr, nullptr, nullptr, nullptr, -1, kBypassModeOptions, sizeof(kBypassModeOptions) / sizeof(kBypassModeOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I9", HaEntityPlatform::Number, HaEntitySourceType::Setting, "Hysterese bypass", "I9", "I9", "number.renovent_i9", false, true, nullptr, nullptr, nullptr, nullptr, -1, nullptr, 0, true, 0.0f, true, 5.0f, true, 1.0f, "box"},
      {"I10", HaEntityPlatform::Select, HaEntitySourceType::Setting, "Constante druk uitgeschakeld", "I10", "I10", "select.renovent_i10", false, true, nullptr, nullptr, nullptr, nullptr, -1, kBinaryOptions, sizeof(kBinaryOptions) / sizeof(kBinaryOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I11", HaEntityPlatform::Select, HaEntitySourceType::Setting, "Voor- of naverwarmer", "I11", "I11", "select.renovent_i11", false, true, nullptr, nullptr, nullptr, nullptr, -1, kHeaterTypeOptions, sizeof(kHeaterTypeOptions) / sizeof(kHeaterTypeOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I12", HaEntityPlatform::Number, HaEntitySourceType::Setting, "Offset temperatuur voorverwarmer", "I12", "I12", "number.renovent_i12", false, true, nullptr, "temperature", nullptr, "°C", 1, nullptr, 0, true, -30.0f, true, 30.0f, true, 0.5f, "box"},
      {"I13", HaEntityPlatform::Select, HaEntitySourceType::Setting, "Filtermelding aan/uit", "I13", "I13", "select.renovent_i13", false, true, nullptr, nullptr, nullptr, nullptr, -1, kBinaryOptions, sizeof(kBinaryOptions) / sizeof(kBinaryOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I14", HaEntityPlatform::Select, HaEntitySourceType::Setting, "Optieprint aanwezig", "I14", "I14", "select.renovent_i14", false, true, nullptr, nullptr, nullptr, nullptr, -1, kBinaryOptions, sizeof(kBinaryOptions) / sizeof(kBinaryOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I15", HaEntityPlatform::Select, HaEntitySourceType::Setting, "WTW configuratie", "I15", "I15", "select.renovent_i15", false, true, nullptr, nullptr, nullptr, nullptr, -1, kWtwConfigurationOptions, sizeof(kWtwConfigurationOptions) / sizeof(kWtwConfigurationOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I16", HaEntityPlatform::Select, HaEntitySourceType::Setting, "Ventilator uit", "I16", "I16", "select.renovent_i16", false, true, nullptr, nullptr, nullptr, nullptr, -1, kFanOffOptions, sizeof(kFanOffOptions) / sizeof(kFanOffOptions[0]), false, 0.0f, false, 0.0f, false, 0.0f, nullptr},
      {"I17", HaEntityPlatform::Number, HaEntitySourceType::Setting, "Repeatertijd", "I17", "I17", "number.renovent_i17", false, true, nullptr, "duration", nullptr, "h", 0, nullptr, 0, true, 1.0f, true, 24.0f, true, 1.0f, "box"},
      {"I18", HaEntityPlatform::Number, HaEntitySourceType::Setting, "Maximale uitschakeltijd ventilator(en)", "I18", "I18", "number.renovent_i18", false, true, nullptr, "duration", nullptr, "s", 0, nullptr, 0, true, 1.0f, true, 240.0f, true, 1.0f, "box"},
      {"I19", HaEntityPlatform::Number, HaEntitySourceType::Setting, "Minimale uitschakeltijd ventilator(en) na 230V", "I19", "I19", "number.renovent_i19", false, true, nullptr, "duration", nullptr, "s", 0, nullptr, 0, true, 1.0f, true, 240.0f, true, 1.0f, "box"},
  };

} // namespace

const HaRootDefinition &getHaRootDefinition()
{
  return kHaRootDefinition;
}

size_t getHaEntityDefinitionCount()
{
  return sizeof(kHaEntityDefinitions) / sizeof(kHaEntityDefinitions[0]);
}

const HaEntityDefinition *getHaEntityDefinitionAt(size_t index)
{
  if (index >= getHaEntityDefinitionCount())
  {
    return nullptr;
  }

  return &kHaEntityDefinitions[index];
}

const HaEntityDefinition *findHaEntityDefinitionByKey(const char *key)
{
  if (key == nullptr)
  {
    return nullptr;
  }

  for (size_t index = 0; index < getHaEntityDefinitionCount(); ++index)
  {
    const HaEntityDefinition &definition = kHaEntityDefinitions[index];
    if (std::strcmp(definition.key, key) == 0)
    {
      return &definition;
    }
  }

  return nullptr;
}