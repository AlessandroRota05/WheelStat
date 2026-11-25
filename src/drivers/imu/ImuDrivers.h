#pragma once

// WheelStat - src/drivers/imu/ImuDrivers.h
// Internal to the IMU drivers: what each chip family file provides, the
// list Imu.cpp walks at boot, and the gyro bias measurement shared by the
// chips that need it.
//
// Adding an IMU: a new file here defining one Driver, and one line in the
// list in Imu.cpp.

#include <Arduino.h>

#include "../Imu.h"

namespace imu {

struct Driver {
  const char *(*detect)();          // name if found and set up, nullptr if absent
  bool (*read)(Reading &out);
  uint8_t (*calibrationLevel)();
  void (*printCalibration)(Print &out);
};

extern const Driver BNO055;     // 0x28 0x29, fusion in the chip (library)
extern const Driver BNO08X;     // 0x4A 0x4B, BNO085/086 (library)
extern const Driver ICM20948;   // 0x68 0x69
extern const Driver MPU6050;    // 0x68 0x69, also MPU6500/9250, ICM-20602/20689
extern const Driver LSM6DS;     // 0x6A 0x6B, LSM6DS3/DSL/DSO/DSOX, ISM330DHCX

// A gyroscope reads a few degrees per second even when still, and that
// error, integrated, becomes a drifting lean. Averaged over a second at
// boot - with the box still, as it is on the boot screen - and subtracted
// from then on. readRawGyro gives rad/s in chip axes.
void measureGyroBias(bool (*readRawGyro)(float gyro[3]), float bias[3]);

// Shared by the drivers that calibrate only the gyro, at boot.
uint8_t readyLevel();
void printGyroBias(Print &out, const float bias[3]);

}  // namespace imu
