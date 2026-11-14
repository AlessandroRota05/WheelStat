#pragma once

/*
  WheelStat - src/core/Gate.h
  ===========================================================================
  A gate is a finish line or a checkpoint: a line through a GPS point, at
  right angles to the direction of travel. A pass is the travel segment
  between two consecutive position reports crossing that line, which also
  says WHERE between the two reports it happened - so a lap time is not
  quantised to the GPS rate.

  The rider only ever marks the point. The direction is learned:
    unknown      nothing known yet. The first pass at speed, in whatever
                 direction, sets it.
    provisional  seen once. A second pass the same way confirms it; a pass
                 the opposite way replaces it (the first one was the ride
                 to the start, not a lap).
    confirmed    passes the other way are ignored, so a return road or a
                 pit lane next to the line does not count.
  ===========================================================================
*/

#include <stdint.h>

#include "Geo.h"

namespace core {

constexpr uint8_t GATE_UNKNOWN     = 0;
constexpr uint8_t GATE_PROVISIONAL = 1;
constexpr uint8_t GATE_CONFIRMED   = 2;

// Stored on flash inside a track: see TRACK_MAGIC in src/Tracks.cpp.
struct Gate {
  double  lat;
  double  lon;
  float   headingDeg;  // direction of travel across the line, compass degrees
  uint8_t confidence;  // GATE_*
};

// One position report from the GPS.
struct GpsSample {
  double   lat;
  double   lon;
  float    speedKmh;
  float    courseDeg;  // course over ground, compass degrees
  uint32_t timeMs;     // monotonic, see core::GpsClock
};

enum class Pass : uint8_t {
  None,
  Forward,   // across a gate of known direction, the right way
  Reversed,  // across a provisional gate, the other way
  Learned,   // first pass over a gate of unknown direction
};

struct Crossing {
  Pass  pass;
  float fraction;  // 0..1 along prev -> curr where the line was crossed
};

Crossing detectCrossing(const Gate &gate, const GpsSample &prev, const GpsSample &curr);

}  // namespace core
