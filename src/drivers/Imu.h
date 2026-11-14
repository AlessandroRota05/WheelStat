#pragma once

/*
  WheelStat - src/drivers/Imu.h
  ===========================================================================
  IMU driver contract. The implementation is picked in Config.h (IMU_*).

  read() delivers four quantities referred to the BIKE, not to the chip:
    lean       degrees of lateral tilt, positive to the RIGHT
    pitch      degrees of pitch, positive NOSE UP
    accelLong  m/s^2, positive under ACCELERATION, gravity removed
    accelLat   m/s^2, positive to the RIGHT
  Translating the sensor axes into these is the driver's job, and it is the
  part that cannot be guessed on paper: it has to be tried on the bench. The
  SIGN_* values in Config.h flip a direction, but swapping two axes is not a
  sign flip and must be solved inside the driver.
  ===========================================================================
*/

#include <Arduino.h>

namespace imu {

bool begin();

// False if the reading failed and the outputs must be ignored.
bool read(float &lean, float &pitch, float &accelLong, float &accelLat);

// 0 = not calibrated, 3 = ready. A chip that calibrates itself returns 3
// straight away and the boot calibration screen closes on its own.
uint8_t calibrationLevel();

// One diagnostic line: which counters exist depends on the chip.
void printCalibration(Print &out);

// Name shown on the boot checklist (about 11 characters at most).
const char *name();

}  // namespace imu
