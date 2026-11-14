#pragma once

/*
  WheelStat - src/LapTimer.h
  ===========================================================================
  Lap and sector timing against a "course": a finish line plus optional
  intermediate checkpoints, each one a line across the track (core::Gate).

  The timing itself is core::LapLogic, which runs on a PC as well; this
  module feeds it the GPS reports and answers in the firmware's own terms.
  It knows nothing about saved tracks: src/Tracks hands it the course to
  time and reacts to the events it reports.
  ===========================================================================
*/

#include <Arduino.h>

#include "Tuning.h"
#include "core/LapLogic.h"

namespace laps {

using Course = core::Course;
using Gate   = core::Gate;
using Event  = core::LapEvent;

// Replaces the course. Lap counters are kept: call resetLaps() when the
// line itself changes.
void setCourse(const Course &course);
const Course &course();

// True once after a pass taught a gate its direction of travel: course()
// has changed and is worth saving.
bool takeCourseChanged();

// Clears laps, sectors and the running stopwatch; the course stays.
void resetLaps();

// Times every GPS report that arrived since the last call. Call every loop,
// after gps::update(). With more than one event in a batch, the most
// important one is returned.
Event update();

// The point being chased right now: a checkpoint if the course has some
// still to pass in THIS lap, otherwise the finish line. Returns true when
// it is the finish line.
bool nextTarget(double &lat, double &lon);

bool     lapRunning();
uint32_t currentLapMs();          // 0 if no lap is running
int      lapCount();              // completed laps
uint32_t lapTime(int lap);
int      bestLapIndex();          // -1 if none
uint32_t bestLapMs();             // 0 if none
uint32_t lastLapMs();             // 0 if none

int      sectorCount();           // checkpoints + 1
uint32_t sectorTime(int lap, int sector);
uint32_t bestSectorMs(int sector);  // session best, 0 if none

// Trace of the last completed lap (0 points before the first one).
int closedLapTrace(const core::TracePoint *&points);

// GPS reports timed so far: it changes when update() saw a new position.
uint32_t reportCount();

// Where the last report was and how far into the lap, on the report's own
// clock. False if no lap is running.
bool lastPosition(double &lat, double &lon, uint32_t &lapMs);

}  // namespace laps
