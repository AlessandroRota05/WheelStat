// STMicroelectronics LSM6DS family at 0x6A (0x6B with SDO high): LSM6DS3,
// LSM6DS3TR-C, LSM6DSL, LSM6DSO, LSM6DSOX and the industrial ISM330DHCX,
// which share the registers used here. Common on Adafruit and SparkFun
// boards; the ISM330DHCX is the most robust to vibration of the lot.

#include "ImuDrivers.h"

#include "../I2cBus.h"

namespace imu {
namespace lsm6ds {

static uint8_t address;
static float   bias[3];

constexpr float ACCEL_SCALE = 0.000244f * 9.80665f;            // 0.244 mg per count at +-8 G
constexpr float GYRO_SCALE  = 0.0175f * 0.0174532925f;         // 17.5 mdps per count at +-500 deg/s

static const char *modelName(uint8_t whoAmI) {
  switch (whoAmI) {
    case 0x69: return "LSM6DS3";
    case 0x6A: return "LSM6DSL";      // also the LSM6DS3TR-C
    case 0x6C: return "LSM6DSO";      // also the LSM6DSOX
    case 0x6B: return "ISM330DHCX";   // also the LSM6DSR
    default:   return nullptr;
  }
}

static int16_t word(const uint8_t *p) { return (int16_t)(p[0] | (p[1] << 8)); }   // little-endian

static bool readBoth(float accel[3], float gyro[3]) {
  uint8_t b[12];   // gyro x y z, then accel x y z
  if (!i2c::readRegisters(address, 0x22, b, 12)) return false;
  for (int k = 0; k < 3; k++) {
    gyro[k]  = word(b + 2 * k) * GYRO_SCALE;
    accel[k] = word(b + 6 + 2 * k) * ACCEL_SCALE;
  }
  return true;
}

static bool readGyroOnly(float gyro[3]) {
  float accel[3];
  return readBoth(accel, gyro);
}

static const char *detect() {
  const char *found = nullptr;
  for (uint8_t at : {0x6A, 0x6B}) {
    found = modelName(i2c::readRegister(at, 0x0F));
    if (found) { address = at; break; }
  }
  if (!found) return nullptr;

  i2c::writeRegister(address, 0x12, 0x01);   // software reset
  delay(20);
  i2c::writeRegister(address, 0x12, 0x44);   // block data update, auto-increment
  i2c::writeRegister(address, 0x10, 0x5C);   // accel 208 Hz, +-8 G
  i2c::writeRegister(address, 0x11, 0x54);   // gyro 208 Hz, +-500 deg/s
  delay(50);

  measureGyroBias(readGyroOnly, bias);
  return found;
}

static bool read(Reading &out) {
  if (!readBoth(out.accel, out.gyro)) return false;
  for (int k = 0; k < 3; k++) out.gyro[k] -= bias[k];
  out.kind = Kind::Raw;
  return true;
}

static void printCalibration(Print &out) { printGyroBias(out, bias); }

}  // namespace lsm6ds

const Driver LSM6DS = { lsm6ds::detect, lsm6ds::read, readyLevel, lsm6ds::printCalibration };

}  // namespace imu
