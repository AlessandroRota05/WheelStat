#pragma once

#include <Arduino.h>

// Non-blocking timer: true (and re-arms) once `intervalMs` has passed since
// the last trigger. Unsigned subtraction survives the millis() rollover.
inline bool elapsed(uint32_t &lastTrigger, uint32_t intervalMs) {
  uint32_t now = millis();
  if (now - lastTrigger >= intervalMs) {
    lastTrigger = now;
    return true;
  }
  return false;
}

// Brings an angle back into -180..+180. Needed after subtracting the zero
// offset: near the sensor's wrap point the result would jump by 360 degrees.
inline float wrapAngle180(float degrees) {
  while (degrees >  180.0f) degrees -= 360.0f;
  while (degrees < -180.0f) degrees += 360.0f;
  return degrees;
}

// "m:ss.d". No hours: a lap that long is off the scale for any circuit.
// Shared by the OLED and the website so the format is defined once.
inline void formatLapTime(uint32_t ms, char *out, size_t size) {
  uint32_t totalSeconds = ms / 1000;
  snprintf(out, size, "%lu:%02lu.%lu",
           (unsigned long)(totalSeconds / 60),
           (unsigned long)(totalSeconds % 60),
           (unsigned long)((ms % 1000) / 100));
}
