#include "SettingsRules.h"

#include <string.h>

#include "../Tuning.h"

namespace core {

// Same order as the Event enum.
static const Limits EVENT_LIMITS[EVENT_COUNT] = {
  {  5.0f, 60.0f, 15.0f },   // wheelie, degrees
  {  5.0f, 45.0f, 10.0f },   // stoppie, degrees
  { 15.0f, 60.0f, 35.0f },   // lean, degrees
  { 0.30f, 1.50f, 0.70f },   // hard braking, G
  { 0.20f, 1.50f, 0.50f },   // hard acceleration, G
};

static const Limits LEAN_ALERT_LIMITS = { 30.0f, 65.0f, LEAN_MAX_IDEAL_DEG };

const Limits &eventLimits(uint8_t event) { return EVENT_LIMITS[event]; }
const Limits &leanAlertLimits() { return LEAN_ALERT_LIMITS; }

static float clamped(float value, const Limits &limits) {
  if (!(value == value)) return limits.fallback;  // NaN
  if (value < limits.min) return limits.min;
  if (value > limits.max) return limits.max;
  return value;
}

void applyDefaults(Settings &settings) {
  settings.logRateHz = LOG_RATE_DEFAULT_HZ;
  for (int e = 0; e < EVENT_COUNT; e++) settings.eventThreshold[e] = EVENT_LIMITS[e].fallback;
  settings.leanAlertDeg = LEAN_ALERT_LIMITS.fallback;
}

static bool printable(const char *text, size_t minLength, size_t maxLength, bool allowSpaces) {
  size_t length = strlen(text);
  if (length < minLength || length > maxLength) return false;
  for (size_t i = 0; i < length; i++) {
    char c = text[i];
    if (c < (allowSpaces ? ' ' : '!') || c > '~') return false;
  }
  return true;
}

bool validWifiName(const char *name) {
  // A leading or trailing space is legal in an SSID and invisible on every
  // screen: not worth the confusion.
  size_t length = strlen(name);
  if (length == 0 || name[0] == ' ' || name[length - 1] == ' ') return false;
  return printable(name, 1, WIFI_NAME_MAX, true);
}

bool validWifiPassword(const char *password) {
  return printable(password, WIFI_PASSWORD_MIN, WIFI_PASSWORD_MAX, false);
}

void sanitize(Settings &settings, const char *defaultWifiName) {
  settings.wifiName[WIFI_NAME_MAX]         = '\0';
  settings.wifiPassword[WIFI_PASSWORD_MAX] = '\0';

  if (!validWifiName(settings.wifiName)) {
    strncpy(settings.wifiName, defaultWifiName, WIFI_NAME_MAX);
    settings.wifiName[WIFI_NAME_MAX] = '\0';
  }
  if (!validWifiPassword(settings.wifiPassword)) settings.wifiPassword[0] = '\0';

  if (settings.logRateHz < LOG_RATE_MIN_HZ) settings.logRateHz = LOG_RATE_MIN_HZ;
  if (settings.logRateHz > LOG_RATE_MAX_HZ) settings.logRateHz = LOG_RATE_MAX_HZ;

  for (int e = 0; e < EVENT_COUNT; e++)
    settings.eventThreshold[e] = clamped(settings.eventThreshold[e], EVENT_LIMITS[e]);
  settings.leanAlertDeg = clamped(settings.leanAlertDeg, LEAN_ALERT_LIMITS);
}

void generatePassword(char *out, size_t length, uint32_t (*random32)()) {
  static const char ALPHABET[] = "abcdefghjkmnpqrstuvwxyz23456789";
  const uint32_t symbols = sizeof(ALPHABET) - 1;

  for (size_t i = 0; i < length; i++) out[i] = ALPHABET[random32() % symbols];
  out[length] = '\0';
}

}  // namespace core
