// TDK InvenSense ICM-20948 at 0x68 (0x69 with AD0 high) - the successor of
// the MPU9250. Its registers are split into four banks selected through
// register 0x7F; the magnetometer is ignored.

#include "ImuDrivers.h"

#include "../I2cBus.h"

namespace imu {
namespace icm20948 {

static uint8_t address;
static float   bias[3];

constexpr float ACCEL_SCALE = 9.80665f / 4096.0f;                // +-8 G
constexpr float GYRO_SCALE  = (1.0f / 65.5f) * 0.0174532925f;    // +-500 deg/s

static void bank(uint8_t n) { i2c::writeRegister(address, 0x7F, (uint8_t)(n << 4)); }

static int16_t word(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }

static bool readBoth(float accel[3], float gyro[3]) {
  uint8_t b[12];   // accel x y z, gyro x y z (bank 0)
  if (!i2c::readRegisters(address, 0x2D, b, 12)) return false;
  for (int k = 0; k < 3; k++) {
    accel[k] = word(b + 2 * k) * ACCEL_SCALE;
    gyro[k]  = word(b + 6 + 2 * k) * GYRO_SCALE;
  }
  return true;
}

static bool readGyroOnly(float gyro[3]) {
  float accel[3];
  return readBoth(accel, gyro);
}

static const char *detect() {
  bool found = false;
  for (uint8_t at : {0x68, 0x69}) {
    address = at;
    i2c::writeRegister(at, 0x7F, 0x00);   // bank 0, where WHO_AM_I lives
    if (i2c::readRegister(at, 0x00) == 0xEA) { found = true; break; }
  }
  if (!found) return nullptr;

  bank(0);
  i2c::writeRegister(address, 0x06, 0x80);   // reset
  delay(100);
  i2c::writeRegister(address, 0x06, 0x01);   // awake, best clock
  i2c::writeRegister(address, 0x07, 0x00);   // accel and gyro on
  bank(2);
  i2c::writeRegister(address, 0x00, 0x04);   // gyro ~225 samples/s
  i2c::writeRegister(address, 0x01, 0x1B);   // gyro +-500 deg/s, low-pass on
  i2c::writeRegister(address, 0x11, 0x04);   // accel ~225 samples/s
  i2c::writeRegister(address, 0x14, 0x1D);   // accel +-8 G, low-pass on
  bank(0);
  delay(50);

  measureGyroBias(readGyroOnly, bias);
  return "ICM-20948";
}

static bool read(Reading &out) {
  if (!readBoth(out.accel, out.gyro)) return false;
  for (int k = 0; k < 3; k++) out.gyro[k] -= bias[k];
  out.kind = Kind::Raw;
  return true;
}

static void printCalibration(Print &out) { printGyroBias(out, bias); }

}  // namespace icm20948

const Driver ICM20948 = { icm20948::detect, icm20948::read, readyLevel, icm20948::printCalibration };

}  // namespace imu
