#pragma once

/*
  WheelStat - src/Motion.h
  ===========================================================================
  Live attitude and G-forces from the IMU, already referred to the bike and
  corrected for the zero offset. These values are deliberately unfiltered
  (the display must be responsive): the pothole filter in src/Stats only
  protects records and counters.
  ===========================================================================
*/

#include <Arduino.h>

namespace motion {

struct State {
  float lean;      // degrees, signed: + right / - left
  float pitch;     // degrees, signed: + nose up / - nose down
  float leanAbs;   // absolute lean
  float wheelie;   // nose up, >= 0 (0 inside the dead band)
  float stoppie;   // nose down, >= 0 (0 inside the dead band)
  float gLat;      // + right / - left
  float gLong;     // + acceleration / - braking
};

bool begin();
bool available();

// Reads the IMU. False if there is no IMU or the read failed.
bool update();

// Stores the current attitude as the zero point. Do it with the bike upright
// and level: it compensates a box that is not mounted level (only the
// leftover degrees - the big rotation is handled by the axis remap). Lives
// in RAM and is redone at every power-up.
bool zeroHere();

// 0 = not calibrated, 3 = ready (see imu::calibrationLevel).
uint8_t calibrationLevel();
void printCalibration(Print &out);

const State &state();

}  // namespace motion
