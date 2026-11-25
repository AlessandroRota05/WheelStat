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

// A size in the unit that keeps it short: "1184 KB", "245 MB", "29.8 GB".
// The internal flash is counted in KB, a microSD in GB.
inline void formatSize(uint64_t bytes, char *out, size_t size) {
  const uint64_t KB = 1024, MB = KB * 1024, GB = MB * 1024;
  if (bytes >= 10 * GB)     snprintf(out, size, "%lu GB", (unsigned long)(bytes / GB));
  else if (bytes >= GB)     snprintf(out, size, "%.1f GB", bytes / (double)GB);
  else if (bytes >= 10 * MB) snprintf(out, size, "%lu MB", (unsigned long)(bytes / MB));
  else                      snprintf(out, size, "%lu KB", (unsigned long)(bytes / KB));
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
