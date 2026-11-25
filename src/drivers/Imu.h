#pragma once

/*
  WheelStat - src/drivers/Imu.h
  ===========================================================================
  IMU contract. At boot every enabled chip is looked for on the I2C bus and
  the first that answers is used; the chips live one family per file in
  imu/. Which families are compiled in is set in Config.h (IMU_WITH_*):
  the ones written without a library are always there.

  A chip delivers one of two kinds of reading:
    Angles  the chip does the sensor fusion and the driver hands over the
            BIKE's lean and pitch in degrees and the gravity-free
            acceleration (BNO055: the axes are remapped inside the chip).
    Raw     accelerometer (m/s^2, gravity included) and gyroscope (rad/s)
            in the CHIP's axes. src/Motion turns them into the bike's axes
            (IMU_AXES in Config.h) and runs the attitude filter of
            src/core/Ahrs, which also uses the GPS speed.
  ===========================================================================
*/

#include <Arduino.h>

namespace imu {

enum class Kind : uint8_t { Angles, Raw };

struct Reading {
  Kind kind;
  // Angles
  float lean;        // degrees, positive to the RIGHT
  float pitch;       // degrees, positive NOSE UP
  float accelLong;   // m/s^2, gravity removed, positive accelerating
  float accelLat;    // m/s^2, gravity removed, positive to the RIGHT
  // Raw, chip axes
  float accel[3];    // m/s^2, +9.81 on the axis pointing up when still
  float gyro[3];     // rad/s, bias already removed
};

// Looks for a chip. False when none answers.
bool begin();

// False if there is nothing new, or the reading failed.
bool read(Reading &out);

// 0 = not calibrated, 3 = ready. A chip that needs no calibration at boot
// returns 3 straight away and the boot calibration screen closes on its own.
uint8_t calibrationLevel();

// One diagnostic line: which counters exist depends on the chip.
void printCalibration(Print &out);

// Name shown on the boot checklist (about 11 characters at most), or "-".
const char *name();

}  // namespace imu
