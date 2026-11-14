#pragma once

/*
  WheelStat - src/Tracks.h
  ===========================================================================
  User-defined circuits, for lap timing and records kept per track.

  Each track is a pair of files: /TRACK_n.BIN (name, finish line,
  checkpoints, reference lap) and /TRACK_n_REC.BIN (all-time records ON that
  track). With no track selected the device is in "free mode": a finish line
  can still be set from the OLED but lives in RAM only, like its reference
  lap, and records go to /RECORD.BIN.

  This module owns "which course is being timed": it feeds src/LapTimer,
  tells src/Records which file to use, and keeps the reference lap the live
  delta is measured against.
  ===========================================================================
*/

#include <Arduino.h>

#include "LapTimer.h"
#include "Tuning.h"
#include "core/SessionCodec.h"
#include "core/Trace.h"

namespace tracks {

constexpr int NO_TRACK = -1;

// On-flash layout: change it (or anything it contains: core::Course,
// core::Gate, core::TracePoint) and TRACK_MAGIC in Tracks.cpp must change.
struct Track {
  uint32_t magic;
  char     name[core::TRACK_NAME_SIZE];
  laps::Course course;       // finish line and checkpoints
  uint32_t bestLapMs;        // all-time best ON THIS TRACK, 0 = none

  // The fastest lap ever ridden here, point by point: the shape drawn on
  // the website and what the live delta compares against. It is updated as
  // soon as a faster lap is closed, recording or not; bestLapMs only at the
  // end of a recording, like every other record.
  uint32_t referenceLapMs;   // lap time of the stored trace, 0 = none
  uint16_t referencePoints;
  core::TracePoint reference[MAX_TRACE_POINTS];
};

// Starts in free mode and loads its records.
void begin();

int          activeId();   // NO_TRACK in free mode
bool         hasActive();
const Track &active();     // in free mode only the reference lap is meaningful

// Reads a track definition. False if the file is missing or invalid.
bool load(int id, Track &out);

// Makes a track active: loads it, switches to its records and sets its
// course for lap timing. NO_TRACK goes back to free mode. Either way the lap
// counters restart: another track is another line. False if `id` does not
// exist (nothing changes).
bool select(int id);

// Creates a track with the finish line on the current GPS position and
// selects it. Returns its id, or NO_TRACK (no fix, empty name, no free slot).
int createHere(const char *name);

// Deletes a track and its records. If it was active, back to free mode.
void remove(int id);

// Stores the current GPS position as the finish line and restarts the lap
// count. With an active track the new line is saved to it. The reference
// lap is dropped: it started from the old line. False without a fix.
bool setFinishLineHere();

// Adds a checkpoint on the current GPS position to the active track.
bool addCheckpointHere();
void clearCheckpoints();

// Call every loop with what laps::update() returned, even Event::None: it
// saves what the lap timer learned about the gates, takes a faster lap as
// the new reference, and refreshes the live delta.
void onLapEvent(laps::Event event);

// Milliseconds behind (positive) or ahead (negative) of the reference lap
// at the current position. False when there is nothing to compare with: no
// lap running, no reference, no fix, or too far from the reference.
bool delta(int32_t &ms);

// End of a recording: stores the session's best lap if it beats the track's
// all-time best. True if it did.
bool commitBestLap();

}  // namespace tracks
