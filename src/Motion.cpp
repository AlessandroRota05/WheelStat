#include "Motion.h"

#include "../Config.h"
#include "Tuning.h"
#include "Util.h"
#include "drivers/Imu.h"

namespace motion {

static bool  imuOk = false;
static State current = {};
static float leanOffset  = 0.0f;
static float pitchOffset = 0.0f;

bool begin() {
  imuOk = imu::begin();
  return imuOk;
}

bool available() { return imuOk; }

bool update() {
  if (!imuOk) return false;

  float rawLean, rawPitch, accelLong, accelLat;
  if (!imu::read(rawLean, rawPitch, accelLong, accelLat)) return false;

  // Gravity is already removed by the driver, so dividing by g is enough:
  // the values stay correct with the bike leaned over.
  current.gLong = SIGN_G_LONG * accelLong / GRAVITY_MS2;
  current.gLat  = SIGN_G_LAT  * accelLat  / GRAVITY_MS2;

  current.lean    = SIGN_LEAN  * wrapAngle180(rawLean  - leanOffset);
  current.pitch   = SIGN_PITCH * wrapAngle180(rawPitch - pitchOffset);
  current.leanAbs = fabsf(current.lean);

  // Same pitch, two manoeuvres: nose up = wheelie, nose down = stoppie.
  if (fabsf(current.pitch) < PITCH_DEADBAND_DEG) {
    current.wheelie = 0.0f;
    current.stoppie = 0.0f;
  } else {
    current.wheelie = (current.pitch >= 0) ?  current.pitch : 0.0f;
    current.stoppie = (current.pitch <  0) ? -current.pitch : 0.0f;
  }
  return true;
}

bool zeroHere() {
  if (!imuOk) return false;

  float rawLean, rawPitch, accelLong, accelLat;
  if (!imu::read(rawLean, rawPitch, accelLong, accelLat)) return false;

  leanOffset  = rawLean;
  pitchOffset = rawPitch;
  return true;
}

uint8_t calibrationLevel() { return imu::calibrationLevel(); }

void printCalibration(Print &out) { imu::printCalibration(out); }

const State &state() { return current; }

}  // namespace motion
