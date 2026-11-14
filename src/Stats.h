#pragma once

/*
  WheelStat - src/Stats.h
  ===========================================================================
  The live values through the pothole filter: session maxima, the peaks the
  session log samples, event counters. The filter itself is
  core::PeakFilter; this module feeds it from the sensors.
  ===========================================================================
*/

#include <Arduino.h>

#include "Channels.h"

namespace stats {

// Clears maxima, filter window and event counters. Used when a recording
// starts and after the boot calibration.
void reset();

// Feeds the filter with the current live values. Call it after every
// successful IMU read.
void sample();

const float *sessionMax();   // [CHANNEL_COUNT], since the last reset()

// Peaks since the last takeIntervalMax(), for the session log. False (and
// `out` untouched) if the filter has produced nothing new since then, which
// happens when the log runs faster than the filter window.
bool takeIntervalMax(float *out);

uint16_t eventCount(uint8_t event);

}  // namespace stats
