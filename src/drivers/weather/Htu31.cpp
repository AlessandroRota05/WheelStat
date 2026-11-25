// TE HTU31D at 0x40 (0x41 with ADDR high). Recognised by its serial number
// command, which answers three bytes and a CRC.

#include "WeatherDrivers.h"

#include "../../core/SensorMath.h"
#include "../I2cBus.h"

namespace weatherSensor {

static uint8_t address;
static bool    pending = false;

static bool serialAt(uint8_t at) {
  const uint8_t readSerial = 0x0A;
  uint8_t b[4];
  if (!i2c::write(at, &readSerial, 1) || !i2c::read(at, b, 4)) return false;
  return core::crc8(b, 3) == b[3];
}

static const char *detect() {
  if (serialAt(0x40)) address = 0x40;
  else if (serialAt(0x41)) address = 0x41;
  else return nullptr;
  return "HTU31D";
}

static bool read(float &temperature, float &humidity) {
  bool got = false;
  if (pending) {
    const uint8_t readBoth = 0x00;
    uint8_t b[6];
    if (i2c::write(address, &readBoth, 1) && i2c::read(address, b, 6) &&
        core::crc8(b, 2) == b[2] && core::crc8(b + 3, 2) == b[5]) {
      temperature = core::htu31Temperature(core::be16(b));
      humidity    = core::htu31Humidity(core::be16(b + 3));
      got = true;
    }
  }
  const uint8_t convert = 0x5E;   // both quantities, highest resolution
  pending = i2c::write(address, &convert, 1);
  return got;
}

const Driver HTU31 = { detect, read, 1000 };

}  // namespace weatherSensor
