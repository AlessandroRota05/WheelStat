/*
  BNO055 driver. The chip runs the sensor fusion itself, so ready-made
  angles and gravity-free acceleration come out of here: that is why the
  firmware contains no fusion filter. Everything BNO055-specific is in this
  file, so a driver for its successor stays a contained job
  (docs/HARDWARE.md).
*/

#include "Imu.h"

#include "../../Config.h"

#ifdef IMU_BNO055

#include <Wire.h>
#include <Adafruit_BNO055.h>

// Builds REMAP_CONFIG_Px / REMAP_SIGN_Px from the position chosen in
// Config.h, so it is set in one place instead of two.
#define WS_PASTE2(a, b) a##b
#define WS_PASTE(a, b)  WS_PASTE2(a, b)

namespace imu {

// The first argument (55) is an arbitrary library id, not the address.
static Adafruit_BNO055 bno(55, I2C_ADDR_BNO055, &Wire);

// NDOF: full 9-axis fusion, absolute attitude with no drift. The in-chip
// remap makes the sensor behave as if mounted flat with X towards the front
// of the bike. On a chip without this feature the same rotation would have
// to be done in software, here.
bool begin() {
  if (!bno.begin(OPERATION_MODE_NDOF)) return false;
  bno.setAxisRemap(Adafruit_BNO055::WS_PASTE(REMAP_CONFIG_, IMU_BNO055_REMAP));
  bno.setAxisSign(Adafruit_BNO055::WS_PASTE(REMAP_SIGN_, IMU_BNO055_REMAP));
  bno.setExtCrystalUse(true);
  delay(100);
  return true;
}

// THE AXIS TRANSLATION IS HERE: in the Euler vector of the BNO055 the
// library's .z is the lateral tilt and .y the nose-up rotation, the reverse
// of what the aeronautical names suggest. So .z -> lean and .y -> pitch.
// It cannot be read off the datasheet at a glance: an early version had
// them swapped and it only showed up on the bench. A driver for another
// chip must redo that test, not copy these lines.
//
// The library does not report read errors (a silent chip returns zeros):
// the real check is done once, at boot.
bool read(float &lean, float &pitch, float &accelLong, float &accelLat) {
  sensors_event_t orientation, linearAccel;
  bno.getEvent(&orientation, Adafruit_BNO055::VECTOR_EULER);        // degrees, fused
  bno.getEvent(&linearAccel, Adafruit_BNO055::VECTOR_LINEARACCEL);  // m/s^2, no gravity

  lean      = orientation.orientation.z;
  pitch     = orientation.orientation.y;
  accelLong = linearAccel.acceleration.x;
  accelLat  = linearAccel.acceleration.y;
  return true;
}

// Of the four BNO055 counters only the magnetometer matters: it is the slow
// one, and this chip starts uncalibrated at every power-up.
uint8_t calibrationLevel() {
  uint8_t sys, gyro, accel, mag;
  bno.getCalibration(&sys, &gyro, &accel, &mag);
  return mag;
}

void printCalibration(Print &out) {
  uint8_t sys, gyro, accel, mag;
  bno.getCalibration(&sys, &gyro, &accel, &mag);
  out.printf("Calibr.  : SYS=%u GYRO=%u ACC=%u MAG=%u  (0=no, 3=ok)\n", sys, gyro, accel, mag);
}

const char *name() { return "BNO055"; }

}  // namespace imu

#endif  // IMU_BNO055
