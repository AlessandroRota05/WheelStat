#pragma once

/*
  WheelStat - src/SessionFormat.h
  ===========================================================================
  The vocabulary of a session log, shared by its writer (src/Recorder) and
  its readers (src/Sessions, the website). Always English, whatever the
  interface language, so files stay comparable between devices.

  On flash a session is a compact binary LOG_n.BIN (layout in
  src/core/SessionCodec.h). The website exports it as CSV: the samples,
  then - after a blank line - the session summary:

    Time_s,Lean,Pitch,G_Lat,G_Long,Speed_Kmh,Temp_C,Hum_%,Risk_%,Lat,Lon,Lap
    0.00,-12.4,0.0,-0.31,0.12,64.2,18.4,62,14,45.4641100,9.1885400,1
    ...

    TRACK,<track name or "free">
    SUMMARY,<channels...>,Duration_s
    MAX,48.7,...,1384
    EVENTS,Wheelies>15,Stoppies>10,Leans>35,HardBrakes>0.7G,HardAccels>0.5G
    TOTAL,3,0,17,9,12

    LAPS,Time_s[,Sector_1,Sector_2,...]
    Lap_1,94.20*[,31.10,30.05,33.05]        (* = session best)

  A recording that lost power has no summary: its export stops after the
  samples.
  ===========================================================================
*/

namespace sessionFormat {

constexpr char LOG_PREFIX[]  = "LOG_";
constexpr char LOG_PATTERN[] = "/LOG_%d.BIN";
constexpr char LOG_EXTENSION[] = ".BIN";

// Logs written by firmware up to v9.16 (one text row per minute). They can
// still be downloaded and deleted, nothing more; and their numbers are not
// reused while they are there.
constexpr char LEGACY_LOG_PATTERN[] = "/LOG_%d.CSV";

constexpr char ROW_TRACK[]   = "TRACK";
constexpr char ROW_SUMMARY[] = "SUMMARY";
constexpr char ROW_MAX[]     = "MAX";
constexpr char ROW_EVENTS[]  = "EVENTS";
constexpr char ROW_TOTAL[]   = "TOTAL";
constexpr char ROW_LAPS[]    = "LAPS";
constexpr char LAP_PREFIX[]  = "Lap_";

// Track name written for a session recorded with no track selected.
constexpr char FREE_TRACK[] = "free";

}  // namespace sessionFormat
