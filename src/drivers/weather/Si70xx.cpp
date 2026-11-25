// Silicon Labs Si7021 and the compatible HTU21D / SHT21 at 0x40. The same
// two commands work on all of them; only the Si7021 knows the "electronic
// ID" one, which is how the name on the boot screen is chosen.

#include "WeatherDrivers.h"

#include "../../core/SensorMath.h"
#include "../I2cBus.h"

namespace weatherSensor {

constexpr uint8_t ADDRESS = 0x40;
constexpr uint8_t MEASURE_HUMIDITY    = 0xF5;   // no clock stretching
constexpr uint8_t MEASURE_TEMPERATURE = 0xF3;

// The two quantities are measured one at a time: each call collects one and
// starts the other, so each is refreshed every other call.
static uint8_t pending = 0;   // the command whose result is awaited, 0 = none

// Two data bytes and a CRC; the two low bits are status, not data.
static bool readWord(uint16_t &raw) {
  uint8_t b[3];
  if (!i2c::read(ADDRESS, b, 3) || core::crc8(b, 2) != b[2]) return false;
  raw = core::be16(b) & 0xFFFC;
  return true;
}

static const char *detect() {
  // The user register: present on all of them, absent on the HTU31D.
  uint8_t user;
  const uint8_t readUser = 0xE7;
  if (!i2c::write(ADDRESS, &readUser, 1) || !i2c::read(ADDRESS, &user, 1)) return nullptr;
  if (user == 0xFF || user == 0x00) return nullptr;   // a floating answer

  const uint8_t electronicId[2] = {0xFA, 0x0F};
  uint8_t id[8];
  bool isSi7021 = i2c::write(ADDRESS, electronicId, 2) && i2c::read(ADDRESS, id, 8);
  return isSi7021 ? "Si7021" : "HTU21D";
}

static bool read(float &temperature, float &humidity) {
  bool got = false;
  uint16_t raw;
  if (pending != 0 && readWord(raw)) {
    if (pending == MEASURE_HUMIDITY) humidity = core::si7021Humidity(raw);
    else temperature = core::si7021Temperature(raw);
    got = true;
  }
  uint8_t next = pending == MEASURE_TEMPERATURE ? MEASURE_HUMIDITY : MEASURE_TEMPERATURE;
  pending = i2c::write(ADDRESS, &next, 1) ? next : 0;
  return got;
}

// Half the usual interval: each call refreshes only one of the two.
const Driver SI70XX = { detect, read, 500 };

}  // namespace weatherSensor
