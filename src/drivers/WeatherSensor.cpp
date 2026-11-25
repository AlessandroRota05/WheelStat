#include "WeatherSensor.h"

#include "weather/WeatherDrivers.h"

namespace weatherSensor {

// The order is the detection order, and it matters where two chips share an
// address: HDC1080, HTU31D and Si70xx all answer at 0x40, and each of their
// detect() functions checks an identity the others do not have - the two
// with a register or a CRC to check go before the Si70xx, whose test is
// the weakest. The DHT is last because it is not on the bus: the only way
// to find it is a slow attempt at reading it.
static const Driver *const CANDIDATES[] = {
  &SENSIRION, &AHT, &HDC1080, &HTU31, &SI70XX, &AM2320, &BME280, &DHT_SENSOR,
};

static const Driver *chosen = nullptr;
static const char   *chosenName = "-";

bool begin() {
  for (const Driver *driver : CANDIDATES) {
    const char *found = driver->detect();
    if (found != nullptr) {
      chosen = driver;
      chosenName = found;
      return true;
    }
  }
  return false;
}

bool read(float &temperature, float &humidity) {
  return chosen != nullptr && chosen->read(temperature, humidity);
}

uint32_t intervalMs() { return chosen != nullptr ? chosen->intervalMs : 2000; }

const char *name() { return chosenName; }

}  // namespace weatherSensor
