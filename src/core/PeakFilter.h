#pragma once

/*
  WheelStat - src/core/PeakFilter.h
  ===========================================================================
  Pothole filter, maxima and event counters.

  The filter keeps, per channel, the MINIMUM inside a short window - the
  level held for the whole window. A pothole lifts the signal for too short
  a time to raise that minimum; a real lean passes through untouched. When
  the window closes, the sustained level feeds the interval maxima (what the
  session log samples), the session maxima and the event counters.
  ===========================================================================
*/

#include <stdint.h>

#include "../Channels.h"

namespace core {

class PeakFilter {
 public:
  // Clears maxima, the open window and the event counters.
  void reset(uint32_t nowMs);

  // Feeds the instantaneous channel values, all positive.
  // `eventThresholds` is [EVENT_COUNT]. True when this call closed a window.
  bool sample(uint32_t nowMs, const float *values, const float *eventThresholds);

  const float *intervalMax() const { return intervalMax_; }  // since clearIntervalMax()
  const float *sessionMax() const  { return sessionMax_; }   // since reset()
  void clearIntervalMax();

  uint16_t eventCount(uint8_t event) const { return eventCount_[event]; }

 private:
  void countEvent(uint8_t event, float level, float threshold);

  float    windowMin_[CHANNEL_COUNT]   = {};  // sustained level of the open window
  float    intervalMax_[CHANNEL_COUNT] = {};
  float    sessionMax_[CHANNEL_COUNT]  = {};
  bool     windowOpen_  = false;
  uint32_t windowStart_ = 0;

  uint16_t eventCount_[EVENT_COUNT]  = {};
  bool     eventActive_[EVENT_COUNT] = {};  // "manoeuvre in progress", for hysteresis
};

}  // namespace core
