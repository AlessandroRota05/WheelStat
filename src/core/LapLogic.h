#pragma once

/*
  WheelStat - src/core/LapLogic.h
  ===========================================================================
  Lap and sector timing against a "course": a finish line plus optional
  checkpoints, each one a core::Gate. Fed with one GPS report at a time, it
  knows nothing about the receiver, the clock or the flash: src/LapTimer
  wraps it for the firmware.

  Times come from where each gate was crossed BETWEEN two reports, on the
  clock the reports carry, so they do not depend on how often or how
  regularly onFix() gets called.
  ===========================================================================
*/

#include <stdint.h>

#include "../Tuning.h"
#include "Gate.h"
#include "Trace.h"

namespace core {

// Stored on flash inside a track: see TRACK_MAGIC in src/Tracks.cpp.
struct Course {
  bool    finishSet;
  Gate    finish;
  uint8_t checkpointCount;  // 0..MAX_CHECKPOINTS
  Gate    checkpoints[MAX_CHECKPOINTS];
};

// In increasing order of importance: when one batch of reports produces
// more than one, the highest is the one worth reporting.
enum class LapEvent : uint8_t {
  None,
  Started,   // first pass over the line: the stopwatch is armed
  Sector,    // an intermediate checkpoint was passed
  Lap,       // a lap was completed
  BestLap,   // a lap was completed and it is the session best
};

class LapLogic {
 public:
  // Replaces the course. Lap counters are kept: call reset() when the line
  // itself changes.
  void setCourse(const Course &course) { course_ = course; }
  const Course &course() const { return course_; }

  // Clears laps, sectors and the running stopwatch; the course stays.
  void reset();

  // Call with every NEW position report, in order.
  LapEvent onFix(const GpsSample &fix);

  // True once after a pass taught a gate its direction: the course has
  // changed and whoever stores it should save it.
  bool takeCourseChanged();

  // The gate being chased right now: a checkpoint if the course has some
  // still to pass in THIS lap, otherwise the finish line.
  const Gate &nextGate(bool &isFinish) const;

  bool     running() const    { return running_; }
  uint32_t lapStartMs() const { return lapStart_; }  // on the reports' clock

  int      lapCount() const          { return lapCount_; }
  uint32_t lapMs(int lap) const      { return lapMs_[lap]; }
  int      bestLap() const           { return bestLap_; }  // -1 if none
  int      sectorCount() const       { return course_.checkpointCount + 1; }
  uint32_t sectorMs(int lap, int sector) const { return sectorMs_[lap][sector]; }
  uint32_t bestSectorMs(int sector) const      { return bestSectorMs_[sector]; }

  // Trace of the last completed lap (empty before the first one).
  int closedTrace(const TracePoint *&points) const;

 private:
  void closeSector(int sector, uint32_t when);
  TraceRecorder &openTrace() { return traces_[openTrace_]; }

  Course course_ = {};

  GpsSample prev_ = {};
  bool      havePrev_ = false;

  bool     running_ = false;  // true from the first pass onwards
  uint32_t lapStart_ = 0;
  uint32_t sectorStart_ = 0;
  // With no checkpoints this is always "past the last one", so the plain
  // single-sector lap needs no special case.
  int      nextCheckpoint_ = 0;
  bool     courseChanged_ = false;

  uint32_t lapMs_[MAX_LAPS] = {};
  // [lap][sector], indexed like lapMs_ so a lap and its sectors line up.
  uint32_t sectorMs_[MAX_LAPS][MAX_SECTORS] = {};
  uint32_t bestSectorMs_[MAX_SECTORS] = {};
  int      lapCount_ = 0;
  int      bestLap_  = -1;

  // Two buffers: the lap in progress and the last completed one. Closing a
  // lap swaps them instead of copying.
  TraceRecorder traces_[2];
  int           openTrace_ = 0;
  bool          hasClosedTrace_ = false;
};

}  // namespace core
