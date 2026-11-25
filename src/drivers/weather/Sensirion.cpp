// Sensirion SHT4x (SHT40/41/45), SHT3x (SHT30/31/35) and SHTC3.
//
// Same company, same habits: 16-bit commands, answers in 16-bit words each
// followed by its CRC-8. The SHT4x and SHT3x even share address 0x44 (0x45
// with ADDR high on the SHT3x): they are told apart by asking for the
// SHT4x serial number with a command the SHT3x does not know - only a real
// SHT4x answers with two words whose CRCs check out.

#include "WeatherDrivers.h"

#include "../../core/SensorMath.h"
#include "../I2cBus.h"

namespace weatherSensor {

enum class Model : uint8_t { Sht4x, Sht3x, Shtc3 };

static Model   model;
static uint8_t address;
static bool    pending = false;

// Six bytes: two words, each with its CRC. True only if both CRCs match.
static bool readWords(uint16_t &first, uint16_t &second) {
  uint8_t b[6];
  if (!i2c::read(address, b, 6)) return false;
  if (core::crc8(b, 2) != b[2] || core::crc8(b + 3, 2) != b[5]) return false;
  first = core::be16(b);
  second = core::be16(b + 3);
  return true;
}

static bool sht4xAt(uint8_t at) {
  const uint8_t serialCommand = 0x89;
  if (!i2c::write(at, &serialCommand, 1)) return false;
  delay(2);
  address = at;
  uint16_t a, b;
  return readWords(a, b);
}

static bool sht3xAt(uint8_t at) {
  if (!i2c::command16(at, 0x3780)) return false;  // read serial number
  delay(2);
  address = at;
  uint16_t a, b;
  return readWords(a, b);
}

// The SHTC3 sleeps between measurements: wake it, ask its ID.
static bool shtc3() {
  if (!i2c::command16(0x70, 0x3517)) return false;   // wake up
  delayMicroseconds(300);
  if (!i2c::command16(0x70, 0xEFC8)) return false;   // read ID register
  uint8_t b[3];
  if (!i2c::read(0x70, b, 3) || core::crc8(b, 2) != b[2]) return false;
  i2c::command16(0x70, 0xB098);                      // back to sleep
  return (core::be16(b) & 0x083F) == 0x0807;
}

static const char *detect() {
  if (sht4xAt(0x44)) { model = Model::Sht4x; return "SHT4x"; }
  if (sht3xAt(0x44) || sht3xAt(0x45)) { model = Model::Sht3x; return "SHT3x"; }
  if (shtc3()) { model = Model::Shtc3; address = 0x70; return "SHTC3"; }
  return nullptr;
}

static void startMeasurement() {
  switch (model) {
    case Model::Sht4x: {
      // High repeatability, and the heater left OFF: it clears condensation
      // but warms the chip by several degrees, and air temperature is half
      // of the grip risk formula.
      const uint8_t measure = 0xFD;
      pending = i2c::write(address, &measure, 1);
      break;
    }
    case Model::Sht3x:
      pending = i2c::command16(address, 0x2400);  // high repeatability, no clock stretching
      break;
    case Model::Shtc3:
      i2c::command16(address, 0x3517);
      delayMicroseconds(300);
      pending = i2c::command16(address, 0x7866);   // normal mode, temperature first
      break;
  }
}

static bool read(float &temperature, float &humidity) {
  bool got = false;
  uint16_t rawT, rawH;
  if (pending && readWords(rawT, rawH)) {
    temperature = core::sensirionTemperature(rawT);
    humidity = model == Model::Sht4x ? core::sht4xHumidity(rawH) : core::sht3xHumidity(rawH);
    got = true;
  }
  if (model == Model::Shtc3) i2c::command16(address, 0xB098);   // sleep until next time
  startMeasurement();
  return got;
}

const Driver SENSIRION = { detect, read, 1000 };

}  // namespace weatherSensor
