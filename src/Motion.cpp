#include "Motion.h"

#include "../Config.h"
#include "Gps.h"
#include "Tuning.h"
#include "Util.h"
#include "core/Ahrs.h"
#include "core/AxisMap.h"
#include "drivers/Imu.h"

namespace motion {

static bool  imuOk = false;
static State current = {};
static float leanOffset  = 0.0f;
static float pitchOffset = 0.0f;
static float lastLean = 0.0f, lastPitch = 0.0f;   // before the zero offsets

// For the IMUs without fusion of their own.
static core::Ahrs ahrs;
static uint32_t   lastMicros = 0;
static constexpr core::AxisMap MOUNT = IMU_AXES;

bool begin() {
  imuOk = imu::begin();
  lastMicros = micros();
  return imuOk;
}

bool available() { return imuOk; }

// The raw path: chip axes to bike axes, then the attitude filter, which
// takes the GPS speed to remove what cornering and braking add to the
// accelerometer (see src/core/Ahrs.h).
static core::Attitude fuse(const imu::Reading &r) {
  uint32_t now = micros();
  float dt = (now - lastMicros) * 1e-6f;
  lastMicros = now;
  if (dt <= 0.0f || dt > 0.05f) dt = 0.005f;   // first call, or after a long block (web page)

  const gps::Fix &position = gps::fix();
  ahrs.setSpeed(position.valid ? position.speedKmh / 3.6f : 0.0f, millis() * 1e-3f);

  float accel[3], gyro[3];
  MOUNT.apply(r.accel, accel);
  MOUNT.apply(r.gyro, gyro);
  return ahrs.update(gyro, accel, dt);
}

bool update() {
  if (!imuOk) return false;

  imu::Reading r;
  if (!imu::read(r)) return false;

  float accelLong, accelLat;
  if (r.kind == imu::Kind::Angles) {
    lastLean  = r.lean;
    lastPitch = r.pitch;
    accelLong = r.accelLong;
    accelLat  = r.accelLat;
  } else {
    core::Attitude a = fuse(r);
    lastLean  = a.leanDeg;
    lastPitch = a.pitchDeg;
    accelLong = a.accelLong;
    accelLat  = a.accelLat;
  }

  // Gravity is already out of the accelerations, so dividing by g is
  // enough: the values stay correct with the bike leaned over.
  current.gLong = accelLong / GRAVITY_MS2;
  current.gLat  = accelLat  / GRAVITY_MS2;

  current.lean    = wrapAngle180(lastLean  - leanOffset);
  current.pitch   = wrapAngle180(lastPitch - pitchOffset);
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

// The attitude right now becomes zero: for a box not mounted level.
bool zeroHere() {
  if (!imuOk || !update()) return false;
  leanOffset  = lastLean;
  pitchOffset = lastPitch;
  return true;
}

uint8_t calibrationLevel() { return imu::calibrationLevel(); }

void printCalibration(Print &out) { imu::printCalibration(out); }

const State &state() { return current; }

}  // namespace motion
