// Bosch BME280 at 0x76 (0x77 with SDO high). It also measures pressure,
// which the firmware does not use.
//
// The BMP280 looks the same and sits on the same address but has NO
// humidity sensor - and humidity is half of the grip risk. It is one of
// the most common wrong parts sold online, so it is recognised (chip ID
// 0x58 instead of 0x60) and explained on the serial monitor rather than
// silently ignored.

#include "WeatherDrivers.h"

#include "../../core/SensorMath.h"
#include "../I2cBus.h"

namespace weatherSensor {

static uint8_t address;
static core::Bme280Calibration calibration;

static bool chipAt(uint8_t at) {
  uint8_t id = i2c::readRegister(at, 0xD0, 0);
  if (id == 0x58) {
    Serial.printf("  0x%02X: a BMP280, not a BME280: no humidity, so no grip risk.\n", at);
    return false;
  }
  return id == 0x60;
}

static const char *detect() {
  if (chipAt(0x76)) address = 0x76;
  else if (chipAt(0x77)) address = 0x77;
  else return nullptr;

  uint8_t block1[26], block2[7];
  if (!i2c::readRegisters(address, 0x88, block1, 26) || !i2c::readRegisters(address, 0xE1, block2, 7))
    return nullptr;
  calibration = core::bme280Calibration(block1, block2);

  // Continuous measurement once a second: humidity x1 (must be written
  // before ctrl_meas to take effect), temperature and pressure x1, normal
  // mode, 1000 ms standby. Reading is then just fetching the latest.
  i2c::writeRegister(address, 0xF2, 0x01);
  i2c::writeRegister(address, 0xF5, 0xA0);
  i2c::writeRegister(address, 0xF4, 0x27);
  return "BME280";
}

static bool read(float &temperature, float &humidity) {
  uint8_t b[8];   // pressure (3), temperature (3), humidity (2)
  if (!i2c::readRegisters(address, 0xF7, b, 8)) return false;
  int32_t adcT = ((int32_t)b[3] << 12) | ((int32_t)b[4] << 4) | (b[5] >> 4);
  int32_t adcH = ((int32_t)b[6] << 8) | b[7];
  if (adcT == 0x80000) return false;   // "no measurement yet"
  int32_t tFine;
  temperature = core::bme280Temperature(calibration, adcT, tFine);
  humidity    = core::bme280Humidity(calibration, adcH, tFine);
  return true;
}

const Driver BME280 = { detect, read, 1000 };

}  // namespace weatherSensor
