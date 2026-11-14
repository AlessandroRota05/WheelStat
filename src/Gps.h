#pragma once

/*
  WheelStat - src/Gps.h
  ===========================================================================
  Position, speed and fix state from the NMEA stream. Works with any
  receiver; the module-specific part is src/drivers/GpsModule.

  Two views of the same data: fix() is "where am I now" for whoever just
  wants the latest values; nextSample() hands out every position report
  exactly once, in order and with its own timestamp, for lap timing.
  ===========================================================================
*/

#include <Arduino.h>

#include "core/Gate.h"

namespace gps {

// Two states that must be kept apart:
//   modulePresent  the MODULE answers, i.e. it sends NMEA sentences.
//   valid          it has LOCKED onto satellites. This arrives tens of
//                  seconds after power-up and comes and goes all the time
//                  (bends, canopies, tunnels).
// Module unplugged: modulePresent false. Plugged in but with no sky:
// modulePresent true, valid false.
struct Fix {
  bool    modulePresent;
  bool    valid;
  double  lat;         // last valid position, kept when the fix is lost
  double  lon;
  float   speedKmh;    // forced to 0 without a valid fix
  uint8_t satellites;  // useful even before the fix
};

// Opens the UART, configures the module and listens for NMEA. True if a
// module is talking (NOT that it has a fix).
bool begin();

// Drains the serial buffer and refreshes the fix. Call every loop: NMEA
// arrives as a stream, and unread bytes would make the fix stale.
void update();

const Fix &fix();

// The next position report not handed out yet. False when there is none.
bool nextSample(core::GpsSample &out);

// The clock the reports are stamped with (see core::GpsClock), right now.
uint32_t clockMs();

// Metres from the current position. The caller must have checked fix().valid.
float distanceTo(double lat, double lon);

}  // namespace gps
