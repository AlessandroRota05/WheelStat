// Aosong AM2320 at 0x5C. It sleeps to save power and its first transaction
// only wakes it (and is not acknowledged): every read starts with that.

#include "WeatherDrivers.h"

#include "../../core/SensorMath.h"
#include "../I2cBus.h"

namespace weatherSensor {

constexpr uint8_t ADDRESS = 0x5C;

static bool readFrame(float &temperature, float &humidity) {
  i2c::present(ADDRESS);   // wake-up: no ACK expected
  delay(1);
  const uint8_t request[3] = {0x03, 0x00, 0x04};   // read 4 registers from 0
  if (!i2c::write(ADDRESS, request, 3)) return false;
  delay(2);
  uint8_t frame[8];
  return i2c::read(ADDRESS, frame, 8) && core::am2320Decode(frame, temperature, humidity);
}

static const char *detect() {
  float t, h;
  return readFrame(t, h) ? "AM2320" : nullptr;
}

// A few milliseconds in all: not worth splitting in two phases.
static bool read(float &temperature, float &humidity) {
  return readFrame(temperature, humidity);
}

// The datasheet asks for at least 2 s between reads.
const Driver AM2320 = { detect, read, 2000 };

}  // namespace weatherSensor
