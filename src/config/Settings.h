// Copyright (c) 2026 Sondre Sjølyst
// Persists DeviceConfig to NVS (ESP32 Preferences). Survives power cycles so
// role/wheel and ESP-NOW pairing are remembered across the car's daily on/off.

#ifndef SRC_CONFIG_SETTINGS_H_
#define SRC_CONFIG_SETTINGS_H_

#include "config/DeviceConfig.h"

namespace tyre {

// Loads config from NVS into `out`. If none stored yet, fills defaults and
// persists them. Returns true if a stored config was found.
bool loadConfig(DeviceConfig *out);

// Persists the whole config blob to NVS.
void saveConfig(const DeviceConfig &cfg);

// Clears stored config (factory reset).
void clearConfig();

}  // namespace tyre

#endif  // SRC_CONFIG_SETTINGS_H_
