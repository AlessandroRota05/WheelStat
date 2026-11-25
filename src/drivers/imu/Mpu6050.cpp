// InvenSense MPU6050 and its relatives with the same register map: MPU6500,
// MPU9250/9255 (the magnetometer is ignored), ICM-20602, ICM-20689. At 0x68
// (0x69 with AD0 high). The most common IMU on cheap breakouts (GY-521).
//
// Set to +-8 G and +-500 deg/s: a hard braking is about 1 G, a fast flick
// from one side to the other a couple of hundred degrees per second.

#include "ImuDrivers.h"

#include "../I2cBus.h"

namespace imu {
namespace mpu6050 {

static uint8_t address;
static float   bias[3];

constexpr float ACCEL_SCALE = 9.80665f / 4096.0f;                // m/s^2 per count at +-8 G
constexpr float GYRO_SCALE  = (1.0f / 65.5f) * 0.0174532925f;    // rad/s per count at +-500 deg/s

static const char *modelName(uint8_t whoAmI) {
  switch (whoAmI) {
    case 0x68: return "MPU6050";
    case 0x70: return "MPU6500";
    case 0x71: return "MPU9250";
    case 0x73: return "MPU9255";
    case 0x12: return "ICM-20602";
    case 0x98: return "ICM-20689";
    default:   return nullptr;
  }
}

static int16_t word(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }

static bool readBoth(float accel[3], float gyro[3]) {
  uint8_t b[14];   // accel x y z, temperature, gyro x y z
  if (!i2c::readRegisters(address, 0x3B, b, 14)) return false;
  for (int k = 0; k < 3; k++) {
    accel[k] = word(b + 2 * k) * ACCEL_SCALE;
    gyro[k]  = word(b + 8 + 2 * k) * GYRO_SCALE;
  }
  return true;
}

static bool readGyroOnly(float gyro[3]) {
  float accel[3];
  return readBoth(accel, gyro);
}

static const char *detect() {
  const char *found = nullptr;
  for (uint8_t at : {0x68, 0x69}) {
    found = modelName(i2c::readRegister(at, 0x75));
    if (found) { address = at; break; }
  }
  if (!found) return nullptr;

  i2c::writeRegister(address, 0x6B, 0x80);   // reset
  delay(100);
  i2c::writeRegister(address, 0x6B, 0x01);   // awake, clock from the gyro PLL
  i2c::writeRegister(address, 0x1A, 0x03);   // low-pass ~44 Hz: engine vibration out
  i2c::writeRegister(address, 0x19, 0x04);   // 200 samples a second
  i2c::writeRegister(address, 0x1B, 0x08);   // gyro +-500 deg/s
  i2c::writeRegister(address, 0x1C, 0x10);   // accel +-8 G
  i2c::writeRegister(address, 0x1D, 0x03);   // accel low-pass on the 6500/9250 (ignored by the 6050)
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

}  // namespace mpu6050

const Driver MPU6050 = { mpu6050::detect, mpu6050::read, readyLevel, mpu6050::printCalibration };

}  // namespace imu
