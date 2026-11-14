#pragma once

/*
  WheelStat - src/Settings.h
  ===========================================================================
  What the rider can change from the website without rebuilding: network
  name and password, log rate, event thresholds, lean alert. Kept in
  /SETTINGS.BIN; the rules (defaults, limits) are in src/core/SettingsRules.
  ===========================================================================
*/

#include <Arduino.h>

#include "core/SettingsRules.h"

namespace settings {

// Loads the stored settings, or the defaults if there are none. Call it
// once the storage is mounted.
void load();

const core::Settings &get();

// Forces the values into their limits, puts them in force and stores them.
// False if the write failed: they still apply until power-off.
bool save(const core::Settings &updated);

// Everything except the WiFi credentials back to the defaults.
bool resetToDefaults();

const char *wifiName();

// The access point password. If the device has none yet it makes one up
// and stores it - so call this with the radio already on: the hardware
// random generator is only truly random then.
const char *wifiPassword();

uint8_t      logRateHz();
float        leanAlertDeg();
const float *eventThresholds();  // [EVENT_COUNT]

}  // namespace settings
