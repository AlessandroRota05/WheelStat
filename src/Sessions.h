#pragma once

/*
  WheelStat - src/Sessions.h
  ===========================================================================
  Reading back the session logs stored on flash: listing them, opening one,
  pulling out its summary. Files are read in small pieces into fixed
  buffers, never loaded whole into RAM.
  ===========================================================================
*/

#include <Arduino.h>
#include <LittleFS.h>

#include "Tuning.h"
#include "core/SessionCodec.h"

namespace sessions {

// What the summary of a log says about the session.
struct Info {
  bool     found;               // false if the file has no valid summary
  char     track[core::TRACK_NAME_SIZE];  // track name, or sessionFormat::FREE_TRACK
  uint32_t durationMs;
  int      lapCount;
  uint32_t lapTimes[MAX_LAPS];  // ms
};

struct Entry {
  char name[20];   // file name without the leading slash
  char track[core::TRACK_NAME_SIZE];
  int  lapCount;
};

// An open log, positioned on its first sample.
struct Opened {
  File                file;
  core::SessionHeader header;
  uint32_t            sampleCount;
  bool                hasFooter;  // false: the recording lost power
};

// True for names that belong to session logs, of any format.
bool isLogFile(const char *name);

// True for the logs this firmware writes, as opposed to the text ones left
// by firmware up to v9.16.
bool isCurrentFormat(const char *name);

// Turns a file name received from a client into a safe absolute path. False
// unless it is an existing session log (no other file can be reached).
bool resolvePath(const char *requested, char *path, size_t size);

// Opens a log of the current format. False if it is not one; on success the
// caller closes `opened.file`.
bool open(const char *path, Opened &opened);

// Reads the summary of an open log, leaving the file position wherever it
// ends up. False if the log has none.
bool readFooter(Opened &opened, core::SessionFooter &footer);

// Reads the summary of a log, given its name or path.
void readInfo(const char *nameOrPath, Info &info);

// Lists the stored sessions of the current format (at most
// MAX_SESSIONS_LISTED) in one scan of the flash. The array is owned by the
// module and valid until the next call.
int list(const Entry *&entries);

}  // namespace sessions
