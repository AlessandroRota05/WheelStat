// Aosong AHT10, AHT20, AHT21, AHT25 at 0x38. The cheapest sensors here:
// fine for an indicative grip risk, less so for fine measurements.

#include "WeatherDrivers.h"

#include "../../core/SensorMath.h"
#include "../I2cBus.h"

namespace weatherSensor {

constexpr uint8_t ADDRESS = 0x38;
static bool pending = false;

// The status byte says whether the chip has loaded its calibration. If not,
// it is told to: 0xBE on the AHT2x, 0xE1 on the older AHT10 - each ignores
// the other's command, so both are sent.
static const char *detect() {
  uint8_t status;
  if (!i2c::read(ADDRESS, &status, 1)) return nullptr;
  if (!(status & 0x08)) {
    const uint8_t init20[3] = {0xBE, 0x08, 0x00};
    const uint8_t init10[3] = {0xE1, 0x08, 0x00};
    i2c::write(ADDRESS, init20, 3);
    delay(10);
    i2c::write(ADDRESS, init10, 3);
    delay(10);
  }
  return "AHTx0";
}

static bool read(float &temperature, float &humidity) {
  bool got = false;
  uint8_t frame[6];
  if (pending && i2c::read(ADDRESS, frame, 6)) got = core::ahtDecode(frame, temperature, humidity);

  // The conversion takes 80 ms: collected at the next call.
  const uint8_t trigger[3] = {0xAC, 0x33, 0x00};
  pending = i2c::write(ADDRESS, trigger, 3);
  return got;
}

const Driver AHT = { detect, read, 1000 };

}  // namespace weatherSensor
