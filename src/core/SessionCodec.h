#pragma once

/*
  WheelStat - src/core/SessionCodec.h
  ===========================================================================
  The session log as it sits on flash. One LOG_n.BIN is:

    SessionHeader                       once, when the recording starts
    SessionSample x N                   LOG rate times a second
    SessionFooter                       once, when the recording stops

  A file with no footer is a recording that lost power: header and samples
  are still good, only the summary is missing.

  Everything is little-endian with natural alignment and no padding inside
  the structs (the static_asserts below hold the sizes still). The session
  viewer of the website reads the very same bytes in JavaScript: change a
  struct and src/web/WebSession.cpp must follow, along with SESSION_MAGIC.

  The website turns a log into CSV on download:

    Time_s,Lean,Pitch,G_Lat,G_Long,Speed_Kmh,Temp_C,Hum_%,Risk_%,Lat,Lon,Lap
    0.00,-12.4,0.0,-0.31,0.12,64.2,18.4,62,14,45.4641100,9.1885400,1
    ...
  followed by the summary rows listed in src/SessionFormat.h. Signed
  columns: Lean + right, Pitch + nose up (wheelie), G_Lat + right,
  G_Long + accelerating.
  ===========================================================================
*/

#include <stddef.h>
#include <stdint.h>

#include "../Channels.h"
#include "../Tuning.h"

namespace core {

constexpr uint32_t SESSION_MAGIC        = 0x314C5357;  // "WSL1" as bytes on flash
constexpr uint32_t SESSION_FOOTER_MAGIC = 0x31465357;  // "WSF1"

constexpr size_t TRACK_NAME_SIZE = 24;

struct SessionHeader {
  uint32_t magic;
  uint16_t headerSize;   // the three sizes let a reader skip what it does
  uint16_t sampleSize;   // not know, and are what the website trusts
  uint16_t footerSize;
  uint8_t  rateHz;
  uint8_t  sectorCount;
  char     track[TRACK_NAME_SIZE];
  float    eventThreshold[EVENT_COUNT];  // in force during the session
};

constexpr uint8_t SAMPLE_HAS_FIX = 0x01;

struct SessionSample {
  int32_t  latE7;      // degrees * 1e7, last known position
  int32_t  lonE7;
  int16_t  lean;       // 0.1 deg, + right
  int16_t  pitch;      // 0.1 deg, + nose up
  int16_t  gLat;       // 0.01 G, + right
  int16_t  gLong;      // 0.01 G, + accelerating
  uint16_t speed;      // 0.1 km/h
  int16_t  temperature;  // 0.1 deg C
  uint8_t  humidity;   // %
  uint8_t  lap;        // lap being timed, 0 = none
  uint8_t  flags;      // SAMPLE_*
  uint8_t  reserved;
};

struct SessionFooter {
  uint32_t magic;
  uint32_t sampleCount;
  uint32_t durationMs;
  float    channelMax[CHANNEL_COUNT];
  uint16_t eventCount[EVENT_COUNT];
  uint8_t  lapCount;
  int8_t   bestLap;    // -1 = none
  uint32_t lapMs[MAX_LAPS];
  uint32_t sectorMs[MAX_LAPS][MAX_SECTORS];
};

static_assert(sizeof(SessionSample) == 24, "SessionSample layout changed");
static_assert(sizeof(SessionHeader) == 36 + 4 * EVENT_COUNT, "SessionHeader has padding");

// What one sample is made from, in engineering units. Lean, pitch and the
// two G are the peaks of the pothole filter over the sample interval, with
// their sign back.
struct SampleValues {
  float  lean, pitch, gLat, gLong;
  float  speedKmh;
  float  temperature, humidity;
  double lat, lon;
  bool   hasFix;
  int    lap;
};

SessionSample packSample(const SampleValues &values);
SampleValues  unpackSample(const SessionSample &sample);

// The signed quantity behind a pair of always-positive channel maxima.
inline float signedPeak(float positive, float negative) {
  return positive >= negative ? positive : -negative;
}

// Both write one CSV line, without the line break, and return its length.
// A buffer of CSV_LINE_SIZE is always enough.
constexpr size_t CSV_LINE_SIZE = 128;
int formatCsvHeader(char *out, size_t size);
int formatCsvRow(const SessionSample &sample, uint32_t index, uint8_t rateHz, char *out,
                 size_t size);

// How many samples a log of `fileSize` bytes holds.
uint32_t sampleCountFor(uint32_t fileSize, const SessionHeader &header, bool hasFooter);

}  // namespace core
