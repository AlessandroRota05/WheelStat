#pragma once

/*
  WheelStat - src/Records.h
  ===========================================================================
  All-time records: the highest values ever logged, in a binary file that
  survives power-off. They are updated only at the END of a recording - if
  it was not recorded, it is not a record.

  There is one file per context: free mode has its own, and every saved
  track has one (src/Tracks decides which is open).
  ===========================================================================
*/

#include <Arduino.h>

#include "Channels.h"

namespace records {

struct AllTime {
  uint32_t magic;                    // format signature, see Records.cpp
  float    channels[CHANNEL_COUNT];  // all-time maximum of each channel
  uint16_t sessions;                 // completed recordings
  uint32_t totalMinutes;             // minutes logged over the device's life
};

// Switches to the given records file and loads it. A missing or invalid
// file is not an error: the records simply start from zero.
void open(const char *path);

const AllTime &current();

// Zeroes the open file.
void reset();

// End of a recording: raises the records that were beaten (marking them as
// new), bumps the career counters and saves. Session maxima come out of the
// pothole filter, so records inherit the same protection from spikes.
void commitSession(const float *sessionMax, uint32_t minutes);

// "Just beaten" flags, channel by channel: the asterisks of the end-of-
// session summary.
bool isNew(uint8_t channel);
bool anyNew();
void clearNewFlags();

}  // namespace records
