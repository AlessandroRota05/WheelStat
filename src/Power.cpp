#include "Power.h"

#include "../Config.h"

namespace power {

#ifdef POWER_SENSE_PIN

// Read every 20 ms; three low readings in a row (60 ms) mean the key is off.
// One low reading is not enough: a spark-plug spike or the starter can pull
// a single sample down. A capacitor sized as in docs/HARDWARE.md lasts a few
// hundred milliseconds, so 60 ms still leaves time to write the file.
constexpr uint32_t READ_INTERVAL_MS = 20;
constexpr uint8_t  READINGS_TO_TRIP = 3;

// Back on only clearly above the threshold, so a voltage hovering around it
// does not stop and re-arm over and over.
constexpr float REARM_MARGIN_VOLTS = 1.0f;

static float   lastVolts = NAN;
static uint8_t lowCount  = 0;
static bool    off       = false;
static uint32_t lastRead = 0;

void begin() {
  // 11 dB: the full 0-3.1 V range, which the divider maps up to ~17 V.
  analogSetPinAttenuation(POWER_SENSE_PIN, ADC_11db);
  lastVolts = analogReadMilliVolts(POWER_SENSE_PIN) / 1000.0f * POWER_SENSE_DIVIDER;
}

bool sensing() { return true; }

bool keyTurnedOff() {
  uint32_t now = millis();
  if (now - lastRead < READ_INTERVAL_MS) return false;
  lastRead = now;

  // analogReadMilliVolts() applies the chip's factory calibration: the raw
  // reading of the ESP32 ADC can be off by 10% or more.
  lastVolts = analogReadMilliVolts(POWER_SENSE_PIN) / 1000.0f * POWER_SENSE_DIVIDER;

  if (off) {
    if (lastVolts > POWER_OFF_VOLTS + REARM_MARGIN_VOLTS) off = false;
    lowCount = 0;
    return false;
  }
  lowCount = lastVolts < POWER_OFF_VOLTS ? lowCount + 1 : 0;
  if (lowCount < READINGS_TO_TRIP) return false;
  off = true;
  return true;
}

float volts() { return lastVolts; }

#else

void begin() {}
bool sensing() { return false; }
bool keyTurnedOff() { return false; }
float volts() { return NAN; }

#endif

}  // namespace power
