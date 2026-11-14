#pragma once

/*
  WheelStat - src/core/SettingsRules.h
  ===========================================================================
  What the rider can change from the website, with its defaults and the
  limits every value is forced into. src/Settings stores it on flash.
  ===========================================================================
*/

#include <stddef.h>
#include <stdint.h>

#include "../Channels.h"

namespace core {

// Network name and password are capped at what one row of the OLED shows:
// the display is where a forgotten password is read back, so it must fit.
constexpr size_t WIFI_NAME_MAX     = 12;
constexpr size_t WIFI_PASSWORD_MIN = 8;   // WPA2 refuses less
constexpr size_t WIFI_PASSWORD_MAX = 12;

// Stored on flash: see SETTINGS_MAGIC in src/Settings.cpp.
struct Settings {
  uint32_t magic;
  char     wifiName[WIFI_NAME_MAX + 1];
  // Empty until the device makes one up for itself, the first time the
  // access point is switched on.
  char     wifiPassword[WIFI_PASSWORD_MAX + 1];
  uint8_t  logRateHz;
  float    eventThreshold[EVENT_COUNT];  // degrees, or G
  float    leanAlertDeg;                 // max lean on perfect tarmac
};

struct Limits {
  float min;
  float max;
  float fallback;  // the default
};

const Limits &eventLimits(uint8_t event);
const Limits &leanAlertLimits();

// Everything except the WiFi credentials back to the defaults.
void applyDefaults(Settings &settings);

// Forces every field into its limits: whatever was read from flash or
// typed into the form, what comes out is safe to use. An invalid network
// name becomes `defaultWifiName`; an invalid password is emptied, which
// makes the device generate a new one.
void sanitize(Settings &settings, const char *defaultWifiName);

// Printable ASCII of the right length (the password also without spaces).
bool validWifiName(const char *name);
bool validWifiPassword(const char *password);

// Fills `out` with `length` random characters and a terminator, picked from
// an alphabet with no look-alikes (no 0/O, 1/l/I): it is read off a tiny
// display and typed on a phone.
void generatePassword(char *out, size_t length, uint32_t (*random32)());

}  // namespace core
