// DHT22 / AM2302 and DHT11 on PIN_DHT - one wire, not I2C.
//
// The DHT has no identity to ask for: the only way to know it is there is
// to read it. Which model is also told by the answer: the two send the same
// five bytes but pack them differently, and a DHT11 read as a DHT22 gives
// impossible values (over 100 % humidity), so that is tried second.

#include "WeatherDrivers.h"

#include <DHT.h>

#include "../../../Config.h"

namespace weatherSensor {

static ::DHT dht22(PIN_DHT, DHT22);
static ::DHT dht11(PIN_DHT, DHT11);
static ::DHT *sensor = &dht22;

static bool plausible(float t, float h) {
  return !isnan(t) && !isnan(h) && h >= 0 && h <= 100 && t > -40 && t < 85;
}

// Several attempts: the chip needs about 2 s after power-up before its
// first good value. The second argument of the reads is the library's
// "force": without it close reads return the cached value. A present
// sensor exits at the first pass; an absent one costs about a second.
static const char *detect() {
  const uint8_t  ATTEMPTS         = 4;
  const uint16_t ATTEMPT_PAUSE_MS = 350;

  dht22.begin();
  for (uint8_t attempt = 0; attempt < ATTEMPTS; attempt++) {
    if (attempt > 0) delay(ATTEMPT_PAUSE_MS);
    float t = dht22.readTemperature(false, true);
    float h = dht22.readHumidity();
    if (plausible(t, h)) { sensor = &dht22; return "DHT22"; }
    if (!isnan(t)) {   // it answered, but not like a DHT22
      dht11.begin();
      delay(ATTEMPT_PAUSE_MS);
      if (plausible(dht11.readTemperature(false, true), dht11.readHumidity())) {
        sensor = &dht11;
        return "DHT11";
      }
    }
  }
  return nullptr;
}

static bool read(float &temperature, float &humidity) {
  float t = sensor->readTemperature();
  float h = sensor->readHumidity();
  bool valid = false;
  if (!isnan(t)) { temperature = t; valid = true; }
  if (!isnan(h)) { humidity    = h; valid = true; }
  return valid;
}

// Asked more often than every 2 s, the DHT22 repeats its last reading.
const Driver DHT_SENSOR = { detect, read, 2000 };

}  // namespace weatherSensor
