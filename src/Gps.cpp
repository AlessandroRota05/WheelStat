#include "Gps.h"

#include <TinyGPSPlus.h>

#include "../Config.h"
#include "Tuning.h"
#include "core/GpsClock.h"
#include "drivers/GpsModule.h"

namespace gps {

static TinyGPSPlus parser;
static Fix current = {};
static core::GpsClock fixClock;

// While a web page is being served the loop does not come back here for a
// good fraction of a second. The reports of that time must still be there
// afterwards, each with the receiver's own timestamp: lap times are made of
// them. The default buffer holds barely one; at 10 Hz and 115200 baud
// this one holds well over a second.
constexpr size_t UART_RX_BUFFER = 2048;

// Measured at boot from the rate the module really reached.
static uint32_t fixTimeoutMs = 3000;

// Reports waiting for the lap timer. A handful is plenty: they are taken
// out in the same loop pass that parsed them.
constexpr int QUEUE_SIZE = 8;
static core::GpsSample queue[QUEUE_SIZE];
static int queueHead  = 0;
static int queueCount = 0;

static core::GpsSample lastSample = {};
static bool     haveLastSample = false;
static uint32_t lastTimeStamp  = UINT32_MAX;

// A course older than this says where the bike WAS heading.
constexpr uint32_t COURSE_MAX_AGE_MS = 2000;

// Speed, family and rate are all found by the driver (see GpsModule.h).
bool begin() {
  gpsSerial.setRxBufferSize(UART_RX_BUFFER);  // only takes effect before the UART opens
  delay(200);  // the module must finish its own boot before taking commands
  current.modulePresent = gpsModule::begin();
  fixTimeoutMs = gpsModule::fixTimeoutMs();
  return current.modulePresent;
}

static void enqueue(const core::GpsSample &sample) {
  if (queueCount == QUEUE_SIZE) {  // nobody is taking them: drop the oldest
    queueHead = (queueHead + 1) % QUEUE_SIZE;
    queueCount--;
  }
  queue[(queueHead + queueCount) % QUEUE_SIZE] = sample;
  queueCount++;
}

// A sentence has just been parsed: if it brought a new position, queue it.
static void collectReport() {
  // Only a sentence with a fix updates the location, so this also filters
  // out everything sent while there is no fix.
  if (!parser.location.isUpdated()) return;

  // RMC and GGA of the same instant both carry the position: one report
  // per instant, told apart by the receiver's own time.
  bool     hasTime = parser.time.isValid();
  uint32_t stamp   = hasTime ? parser.time.value() : UINT32_MAX;
  if (hasTime && stamp == lastTimeStamp) return;
  lastTimeStamp = stamp;

  core::GpsSample sample;
  sample.lat      = parser.location.lat();
  sample.lon      = parser.location.lng();
  sample.speedKmh = parser.speed.isValid() ? parser.speed.kmph() : 0.0f;

  if (parser.course.isValid() && parser.course.age() < COURSE_MAX_AGE_MS) {
    sample.courseDeg = parser.course.deg();
  } else if (haveLastSample) {
    sample.courseDeg = (float)TinyGPSPlus::courseTo(lastSample.lat, lastSample.lon,
                                                    sample.lat, sample.lon);
  } else {
    sample.courseDeg = 0.0f;
  }

  int32_t timeOfDayMs = -1;
  if (hasTime) {
    timeOfDayMs = ((parser.time.hour() * 60L + parser.time.minute()) * 60L +
                   parser.time.second()) * 1000L + parser.time.centisecond() * 10L;
  }
  sample.timeMs = fixClock.onFix(timeOfDayMs, millis());

  lastSample     = sample;
  haveLastSample = true;
  enqueue(sample);
}

void update() {
  while (gpsSerial.available() > 0) {
    // A module talking now is present even if it was silent at boot (slow
    // power-up, or plugged in later). Only ever corrects false negatives.
    current.modulePresent = true;
    if (parser.encode(gpsSerial.read())) collectReport();
  }

  // A fix only counts if recent: age() keeps growing until a new valid
  // sentence arrives, so a lost fix expires by itself.
  current.valid = parser.location.isValid() &&
                  parser.location.age() < fixTimeoutMs;

  if (current.valid) {
    current.lat      = parser.location.lat();
    current.lon      = parser.location.lng();
    current.speedKmh = parser.speed.isValid() ? parser.speed.kmph() : 0.0f;
  } else {
    current.speedKmh = 0.0f;  // no fix, no speed: avoids a frozen number
  }

  current.satellites = parser.satellites.isValid() ? parser.satellites.value() : 0;
}

const Fix &fix() { return current; }

bool nextSample(core::GpsSample &out) {
  if (queueCount == 0) return false;
  out = queue[queueHead];
  queueHead = (queueHead + 1) % QUEUE_SIZE;
  queueCount--;
  return true;
}

uint32_t clockMs() { return fixClock.now(millis()); }

float distanceTo(double lat, double lon) {
  return (float)TinyGPSPlus::distanceBetween(current.lat, current.lon, lat, lon);
}

}  // namespace gps
