/*
  Bosch BNO055 at 0x28 (0x29 with the ADR jumper). The chip runs the sensor
  fusion itself, so ready-made angles and gravity-free acceleration come out
  of it. It is the IMU the firmware was built and bench-tested with, which
  is why it keeps its own path (Kind::Angles) instead of the shared filter.

  End of life: see docs/HARDWARE.md for its successors.
*/

#include "../../../Config.h"

#ifdef IMU_WITH_BNO055

#include "ImuDrivers.h"

#include <Adafruit_BNO055.h>
#include <Wire.h>

#include "../I2cBus.h"

// Builds REMAP_CONFIG_Px / REMAP_SIGN_Px from the position chosen in
// Config.h, so it is set in one place instead of two.
#define WS_PASTE2(a, b) a##b
#define WS_PASTE(a, b)  WS_PASTE2(a, b)

namespace imu {
namespace bno055 {

// The first argument (55) is an arbitrary library id, not the address.
static Adafruit_BNO055 at28(55, 0x28, &Wire);
static Adafruit_BNO055 at29(55, 0x29, &Wire);
static Adafruit_BNO055 *bno = &at28;

// NDOF: full 9-axis fusion, absolute attitude with no drift. The in-chip
// remap makes the sensor behave as if mounted flat with X towards the front
// of the bike.
static const char *detect() {
  if (i2c::present(0x28)) bno = &at28;
  else if (i2c::present(0x29)) bno = &at29;
  else return nullptr;

  if (!bno->begin(OPERATION_MODE_NDOF)) return nullptr;
  bno->setAxisRemap(Adafruit_BNO055::WS_PASTE(REMAP_CONFIG_, IMU_BNO055_REMAP));
  bno->setAxisSign(Adafruit_BNO055::WS_PASTE(REMAP_SIGN_, IMU_BNO055_REMAP));
  bno->setExtCrystalUse(true);
  delay(100);
  return "BNO055";
}

// THE AXIS TRANSLATION IS HERE: in the Euler vector of the BNO055 the
// library's .z is the lateral tilt and .y the nose-up rotation, the reverse
// of what the aeronautical names suggest. So .z -> lean and .y -> pitch.
// It cannot be read off the datasheet at a glance: an early version had
// them swapped and it only showed up on the bench. The SIGN_* constants in
// Config.h then fix the directions.
//
// The library does not report read errors (a silent chip returns zeros):
// the real check is done once, at boot.
static bool read(Reading &out) {
  sensors_event_t orientation, linearAccel;
  bno->getEvent(&orientation, Adafruit_BNO055::VECTOR_EULER);        // degrees, fused
  bno->getEvent(&linearAccel, Adafruit_BNO055::VECTOR_LINEARACCEL);  // m/s^2, no gravity

  out.kind      = Kind::Angles;
  out.lean      = SIGN_LEAN   * orientation.orientation.z;
  out.pitch     = SIGN_PITCH  * orientation.orientation.y;
  out.accelLong = SIGN_G_LONG * linearAccel.acceleration.x;
  out.accelLat  = SIGN_G_LAT  * linearAccel.acceleration.y;
  return true;
}

// Of the four BNO055 counters only the magnetometer matters: it is the slow
// one, and this chip starts uncalibrated at every power-up.
static uint8_t calibrationLevel() {
  uint8_t sys, gyro, accel, mag;
  bno->getCalibration(&sys, &gyro, &accel, &mag);
  return mag;
}

static void printCalibration(Print &out) {
  uint8_t sys, gyro, accel, mag;
  bno->getCalibration(&sys, &gyro, &accel, &mag);
  out.printf("Calibr.  : SYS=%u GYRO=%u ACC=%u MAG=%u  (0=no, 3=ok)\n", sys, gyro, accel, mag);
}

}  // namespace bno055

const Driver BNO055 = { bno055::detect, bno055::read, bno055::calibrationLevel, bno055::printCalibration };

}  // namespace imu

#endif  // IMU_WITH_BNO055
