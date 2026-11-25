#include "Ahrs.h"

#include <math.h>

namespace core {

constexpr float G = 9.81f;
constexpr float RAD_TO_DEG_F = 57.2957795f;

// Madgwick's gain: how hard the accelerometer pulls the gyro estimate back.
// Higher follows faster but lets more vibration through.
constexpr float BETA = 0.04f;

// When what is left after the correction is still far from 1 G (a bump, a
// pothole, a wrong speed), the accelerometer is trusted much less for that
// step: the gyro carries the estimate through.
constexpr float TRUST_BAND = 0.15f;   // fraction of G
constexpr float BETA_DISTRUSTED = 0.002f;

// The GPS speed comes in steps a few times a second: its derivative is
// smoothed so one noisy step does not become a jolt of fake braking.
constexpr float DVDT_SMOOTHING = 0.5f;

void Ahrs::reset() {
  q_[0] = 1; q_[1] = q_[2] = q_[3] = 0;
  started_ = false;
  speed_ = 0; speedTime_ = -1; dvdt_ = 0; lastSpeedValue_ = -1;
}

void Ahrs::setSpeed(float speedMs, float timeS) {
  if (speedMs == lastSpeedValue_) return;     // no new GPS value yet
  if (speedTime_ >= 0 && timeS > speedTime_) {
    float raw = (speedMs - speed_) / (timeS - speedTime_);
    dvdt_ += DVDT_SMOOTHING * (raw - dvdt_);
  }
  if (speedMs <= 0) dvdt_ = 0;               // fix lost or standing: no correction
  speed_ = speedMs;
  speedTime_ = timeS;
  lastSpeedValue_ = speedMs;
}

// The first estimate straight from the accelerometer, so the filter does
// not have to crawl there from "level" at boot.
void Ahrs::startFrom(const float a[3]) {
  float roll  = atan2f(a[1], a[2]);
  float pitch = atan2f(-a[0], sqrtf(a[1] * a[1] + a[2] * a[2]));
  float cr = cosf(roll / 2), sr = sinf(roll / 2);
  float cp = cosf(pitch / 2), sp = sinf(pitch / 2);
  q_[0] = cr * cp;
  q_[1] = sr * cp;
  q_[2] = cr * sp;
  q_[3] = -sr * sp;
  started_ = true;
}

Attitude Ahrs::update(const float gyro[3], const float accel[3], float dt) {
  // What the motion puts into the accelerometer (see the header).
  const float v = speed_;
  float f[3] = {
    accel[0] - dvdt_,
    accel[1] - gyro[2] * v,
    accel[2] + gyro[1] * v,
  };

  if (!started_) startFrom(f);

  float q0 = q_[0], q1 = q_[1], q2 = q_[2], q3 = q_[3];
  float gx = gyro[0], gy = gyro[1], gz = gyro[2];

  // Rate of change of the quaternion from the gyroscope
  float qDot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
  float qDot1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy);
  float qDot2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx);
  float qDot3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx);

  float norm = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
  if (norm > 0.0f) {
    float beta = fabsf(norm - G) < TRUST_BAND * G ? BETA : BETA_DISTRUSTED;
    float ax = f[0] / norm, ay = f[1] / norm, az = f[2] / norm;

    // Gradient descent step towards the attitude whose "up" matches the
    // corrected accelerometer (Madgwick 2010, IMU version).
    float _2q0 = 2 * q0, _2q1 = 2 * q1, _2q2 = 2 * q2, _2q3 = 2 * q3;
    float _4q0 = 4 * q0, _4q1 = 4 * q1, _4q2 = 4 * q2;
    float _8q1 = 8 * q1, _8q2 = 8 * q2;
    float q0q0 = q0 * q0, q1q1 = q1 * q1, q2q2 = q2 * q2, q3q3 = q3 * q3;

    float s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
    float s1 = _4q1 * q3q3 - _2q3 * ax + 4 * q0q0 * q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 +
               _8q1 * q2q2 + _4q1 * az;
    float s2 = 4 * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 +
               _8q2 * q2q2 + _4q2 * az;
    float s3 = 4 * q1q1 * q3 - _2q1 * ax + 4 * q2q2 * q3 - _2q2 * ay;
    float sn = sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
    if (sn > 0.0f) {
      qDot0 -= beta * s0 / sn;
      qDot1 -= beta * s1 / sn;
      qDot2 -= beta * s2 / sn;
      qDot3 -= beta * s3 / sn;
    }
  }

  q0 += qDot0 * dt;
  q1 += qDot1 * dt;
  q2 += qDot2 * dt;
  q3 += qDot3 * dt;
  float qn = sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
  q_[0] = q0 / qn; q_[1] = q1 / qn; q_[2] = q2 / qn; q_[3] = q3 / qn;
  q0 = q_[0]; q1 = q_[1]; q2 = q_[2]; q3 = q_[3];

  // "Up" seen from the bike: leaning right it appears towards the bike's
  // left (+y), nose up it appears forward (+x).
  float ux = 2 * (q1 * q3 - q0 * q2);
  float uy = 2 * (q0 * q1 + q2 * q3);
  float uz = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;

  Attitude out;
  out.leanDeg  = atan2f(uy, uz) * RAD_TO_DEG_F;
  out.pitchDeg = atan2f(ux, sqrtf(uy * uy + uz * uz)) * RAD_TO_DEG_F;
  // What the rider feels: the measured force minus gravity's share.
  out.accelLong = accel[0] - G * ux;
  out.accelLat  = -(accel[1] - G * uy);
  return out;
}

}  // namespace core
