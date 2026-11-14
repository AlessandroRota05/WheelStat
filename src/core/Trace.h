#pragma once

/*
  WheelStat - src/core/Trace.h
  ===========================================================================
  The trace of a lap - where the bike was and how far into the lap - and the
  live delta against a reference trace: "at this point of the track the
  reference lap was at 41.2 s, I am at 41.6 s, so I am 0.4 s behind".
  ===========================================================================
*/

#include <stdint.h>

#include "../Tuning.h"

namespace core {

// Stored on flash inside a track: see TRACK_MAGIC in src/Tracks.cpp.
struct TracePoint {
  int32_t  latE7;   // degrees * 1e7
  int32_t  lonE7;
  uint32_t timeMs;  // since the start of the lap
};

// Collects the trace of the lap in progress into a fixed buffer. When the
// buffer fills up it drops every other point and from then on keeps one
// report in two, then one in four...: a lap of any length ends up with
// between half a buffer and a full one, evenly spread over the whole lap.
class TraceRecorder {
 public:
  void clear();
  void add(double lat, double lon, uint32_t timeMs);

  int               count() const  { return count_; }
  const TracePoint *points() const { return points_; }

 private:
  TracePoint points_[MAX_TRACE_POINTS];
  int        count_  = 0;
  uint16_t   stride_ = 1;  // one report in `stride_` is kept
  uint16_t   skip_   = 0;  // reports still to drop before the next kept one
};

class DeltaTracker {
 public:
  // Call at the start of every lap: the search restarts from the first
  // point of the reference.
  void restart() { cursor_ = 0; }

  // Milliseconds ahead (negative) or behind (positive) the reference at the
  // current position, `lapMs` into the lap. False when there is no usable
  // reference or the position is too far from it.
  bool compute(const TracePoint *reference, int count, double lat, double lon,
               uint32_t lapMs, int32_t &deltaMs);

 private:
  // The search looks only around where the last match was. A track often
  // runs close to itself (hairpins, a back straight next to the main one),
  // and the nearest point overall may belong to a different part of the lap.
  int cursor_ = 0;
};

}  // namespace core
