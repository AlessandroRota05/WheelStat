#include "Settings.h"

#include <esp_random.h>

#include "../Config.h"
#include "Storage.h"

namespace settings {

// "WSS1": bump it whenever core::Settings changes.
constexpr uint32_t SETTINGS_MAGIC = 0x57535331;

// Does not start with "LOG_", so the session download/delete endpoints can
// never touch it.
constexpr char SETTINGS_PATH[] = "/SETTINGS.BIN";

// Long enough to resist guessing, short enough to type off the OLED.
constexpr size_t GENERATED_PASSWORD_LENGTH = 10;

static core::Settings current = {};

static bool store() {
  if (storage::saveStruct(SETTINGS_PATH, current, SETTINGS_MAGIC)) return true;
  Serial.println(F("ERROR: could not save the settings."));
  return false;
}

void load() {
  current = core::Settings{};
  core::applyDefaults(current);

  bool stored = storage::loadStruct(SETTINGS_PATH, current, SETTINGS_MAGIC);
  core::sanitize(current, WIFI_DEFAULT_NAME);
  Serial.println(stored ? F("Settings loaded.") : F("No stored settings: using the defaults."));
}

const core::Settings &get() { return current; }

bool save(const core::Settings &updated) {
  current = updated;
  core::sanitize(current, WIFI_DEFAULT_NAME);
  return store();
}

bool resetToDefaults() {
  core::applyDefaults(current);
  return store();
}

const char *wifiName() { return current.wifiName; }

const char *wifiPassword() {
  if (current.wifiPassword[0] == '\0') {
    core::generatePassword(current.wifiPassword, GENERATED_PASSWORD_LENGTH, esp_random);
    store();
    Serial.println(F("Access point password generated."));
  }
  return current.wifiPassword;
}

uint8_t logRateHz() { return current.logRateHz; }

float leanAlertDeg() { return current.leanAlertDeg; }

const float *eventThresholds() { return current.eventThreshold; }

}  // namespace settings
