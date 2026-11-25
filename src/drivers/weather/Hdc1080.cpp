// Texas Instruments HDC1080 at 0x40 - the address of the Si7021 and HTU21D
// too, so it is recognised by its manufacturer register (0x5449, "TI").

#include "WeatherDrivers.h"

#include "../../core/SensorMath.h"
#include "../I2cBus.h"

namespace weatherSensor {

constexpr uint8_t ADDRESS = 0x40;
static bool pending = false;

static const char *detect() {
  uint8_t id[2];
  if (!i2c::readRegisters(ADDRESS, 0xFE, id, 2) || core::be16(id) != 0x5449) return nullptr;
  // Configuration: both quantities in one go, 14 bits each, heater off.
  const uint8_t config[3] = {0x02, 0x10, 0x00};
  i2c::write(ADDRESS, config, 3);
  return "HDC1080";
}

static bool read(float &temperature, float &humidity) {
  bool got = false;
  uint8_t b[4];
  if (pending && i2c::read(ADDRESS, b, 4)) {
    temperature = core::hdc1080Temperature(core::be16(b));
    humidity    = core::hdc1080Humidity(core::be16(b + 2));
    got = true;
  }
  const uint8_t start = 0x00;   // pointing at the temperature starts a conversion
  pending = i2c::write(ADDRESS, &start, 1);
  return got;
}

const Driver HDC1080 = { detect, read, 1000 };

}  // namespace weatherSensor
