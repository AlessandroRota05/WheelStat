#pragma once

/*
  WheelStat - src/core/Ahrs.h
  ===========================================================================
  Attitude from an IMU without sensor fusion of its own (MPU6050, ICM-20948,
  LSM6DS...): a Madgwick filter on gyroscope and accelerometer, which gives
  lean, pitch and the acceleration with gravity taken out.

  On a motorbike the textbook filter is wrong in exactly the moment that
  matters. In a steady corner the bike leans so that the sum of gravity and
  centrifugal force lies in its own plane: the accelerometer then "sees"
  gravity straight down the bike, and a filter trusting it reads the lean
  as zero. The same happens to pitch under braking and acceleration.

  The fix is to take out of the accelerometer what the motion itself puts
  in, known from the GPS speed v and the gyroscope:
      forward   dv/dt               (braking, accelerating)
      sideways  yaw rate * v        (cornering)
      vertical  -pitch rate * v     (crests and dips)
  What is left is gravity, and the filter can trust it again. Without a
  GPS fix the speed is 0 and the correction simply does nothing.

  Axes are the BIKE's: x forward, y left, z up (the driver remaps the chip
  to these). The accelerometer gives specific force in m/s^2 - +9.81 on z
  with the bike upright and still - and the gyroscope rad/s.
  ===========================================================================
*/

#include <stdint.h>

namespace core {

struct Attitude {
  float leanDeg;      // positive to the RIGHT
  float pitchDeg;     // positive NOSE UP
  float accelLong;    // m/s^2, gravity removed, positive accelerating
  float accelLat;     // m/s^2, gravity removed, positive to the RIGHT
};

class Ahrs {
 public:
  // speedMs: GPS ground speed in m/s (0 without a fix); timeS: when it was
  // read, in seconds. Called every update: the derivative is taken only
  // when the GPS value really changes.
  void setSpeed(float speedMs, float timeS);

  // One step: gyro in rad/s, accel in m/s^2 (bike axes), dt in seconds.
  Attitude update(const float gyro[3], const float accel[3], float dt);

  void reset();

 private:
  float q_[4] = {1, 0, 0, 0};    // body -> earth quaternion
  bool  started_ = false;

  float speed_ = 0, speedTime_ = -1, dvdt_ = 0;
  float lastSpeedValue_ = -1;

  void startFrom(const float accel[3]);
};

}  // namespace core
