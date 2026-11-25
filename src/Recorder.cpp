#include "Recorder.h"

#include "Channels.h"
#include "Gps.h"
#include "LapTimer.h"
#include "Records.h"
#include "SessionFormat.h"
#include "Settings.h"
#include "Stats.h"
#include "Storage.h"
#include "Tracks.h"
#include "Tuning.h"
#include "Weather.h"
#include "core/SessionCodec.h"

namespace recorder {

using namespace sessionFormat;

// Samples waiting in RAM for the next write. Sized for the fastest rate
// over one flush interval, with room to spare.
constexpr int PENDING_SAMPLES = 64;

static bool     recording = false;
static File     logFile;
static char     logPath[20] = "";
static uint8_t  rateHz = LOG_RATE_DEFAULT_HZ;
static uint32_t startedAt = 0;
static uint32_t lastDurationMs = 0;

static core::SessionSample pending[PENDING_SAMPLES];
static int      pendingCount  = 0;
static uint32_t samplesTaken  = 0;   // including the ones still in RAM
static uint32_t samplesStored = 0;
static uint32_t lastFlush     = 0;
static uint32_t bytesLeft     = 0;   // what this recording may still write
static bool     writeFailed   = false;

// Peaks of the last interval that had any: repeated when the log runs
// faster than the pothole filter produces them.
static float lastPeaks[CHANNEL_COUNT] = {};

static bool sessionIdTaken(int id) {
  char path[sizeof(logPath)];
  snprintf(path, sizeof(path), LOG_PATTERN, id);
  if (storage::sessions().exists(path)) return true;
  snprintf(path, sizeof(path), LEGACY_LOG_PATTERN, id);
  return storage::sessions().exists(path);
}

// The file stays open for the whole recording and is synced here: if power
// is lost, at most one flush interval goes with it, never the whole file.
static void flush() {
  if (pendingCount == 0) return;

  size_t bytes   = pendingCount * sizeof(core::SessionSample);
  size_t written = logFile.write((const uint8_t *)pending, bytes);
  logFile.flush();

  if (written == bytes) {
    samplesStored += pendingCount;
  } else {
    writeFailed = true;
    Serial.println(F("ERROR: write to flash failed (storage full?)."));
  }
  bytesLeft = bytesLeft > bytes ? bytesLeft - bytes : 0;
  pendingCount = 0;
}

static core::SessionSample currentSample() {
  stats::takeIntervalMax(lastPeaks);

  const weather::Reading &air = weather::reading();
  const gps::Fix &position = gps::fix();

  core::SampleValues values = {};
  values.lean  = core::signedPeak(lastPeaks[CH_LEAN_RIGHT], lastPeaks[CH_LEAN_LEFT]);
  values.pitch = core::signedPeak(lastPeaks[CH_WHEELIE],    lastPeaks[CH_STOPPIE]);
  values.gLat  = core::signedPeak(lastPeaks[CH_GLAT_RIGHT], lastPeaks[CH_GLAT_LEFT]);
  values.gLong = core::signedPeak(lastPeaks[CH_G_ACCEL],    lastPeaks[CH_G_BRAKE]);
  values.speedKmh    = lastPeaks[CH_SPEED];
  values.temperature = air.temperature;
  values.humidity    = air.humidity;

  // The position stays at the last valid fix even if the GPS has just lost
  // it: a last known position is more useful than "0,0", which is in the
  // middle of the ocean. The flag tells the two apart.
  values.lat    = position.lat;
  values.lon    = position.lon;
  values.hasFix = position.valid;
  values.lap    = (laps::course().finishSet && laps::lapRunning()) ? (laps::lapCount() + 1) : 0;

  return core::packSample(values);
}

static void writeFooter() {
  // Static: 860 bytes are too many for the stack.
  static core::SessionFooter footer;
  footer = core::SessionFooter{};
  footer.sampleCount = samplesStored;
  footer.durationMs  = lastDurationMs;

  for (int c = 0; c < CHANNEL_COUNT; c++) footer.channelMax[c] = stats::sessionMax()[c];
  for (int e = 0; e < EVENT_COUNT; e++)   footer.eventCount[e] = stats::eventCount(e);

  int lapCount = laps::lapCount();
  footer.lapCount = (uint8_t)lapCount;
  footer.bestLap  = (int8_t)laps::bestLapIndex();
  for (int lap = 0; lap < lapCount; lap++) {
    footer.lapMs[lap] = laps::lapTime(lap);
    for (int s = 0; s < laps::sectorCount(); s++)
      footer.sectorMs[lap][s] = laps::sectorTime(lap, s);
  }

  footer.magic = core::SESSION_FOOTER_MAGIC;
  if (logFile.write((const uint8_t *)&footer, sizeof(footer)) == sizeof(footer))
    Serial.println(F("Session summary written."));
  else
    Serial.println(F("ERROR: could not write the session summary."));
}

bool start() {
  if (!storage::ensureMounted()) {
    Serial.println(F("ERROR: storage not available."));
    return false;
  }

  uint64_t freeBytes = storage::freeBytes();
  if (freeBytes < MIN_FREE_BYTES) {
    Serial.println(F("ERROR: storage almost full. Delete old logs from the website."));
    return false;
  }

  // Progressive numbering. The cap avoids an endless loop on a corrupted
  // filesystem.
  int sessionId = 1;
  while (sessionIdTaken(sessionId) && sessionId < MAX_SESSION_FILES) sessionId++;

  // Numbering exhausted: better not to start than to silently overwrite.
  if (sessionIdTaken(sessionId)) {
    Serial.println(F("ERROR: log numbering exhausted. Delete old logs from the website."));
    return false;
  }
  snprintf(logPath, sizeof(logPath), LOG_PATTERN, sessionId);

  logFile = storage::sessions().open(logPath, FILE_WRITE);
  if (!logFile) {
    Serial.println(F("ERROR: could not create the log file."));
    return false;
  }

  rateHz = settings::logRateHz();

  core::SessionHeader header = {};
  header.magic       = core::SESSION_MAGIC;
  header.headerSize  = sizeof(core::SessionHeader);
  header.sampleSize  = sizeof(core::SessionSample);
  header.footerSize  = sizeof(core::SessionFooter);
  header.rateHz      = rateHz;
  header.sectorCount = (uint8_t)laps::sectorCount();
  strlcpy(header.track, tracks::hasActive() ? tracks::active().name : FREE_TRACK,
          sizeof(header.track));
  // Thresholds go in the file so the event counts stay readable when it is
  // reopened months later, with other settings.
  for (int e = 0; e < EVENT_COUNT; e++) header.eventThreshold[e] = settings::eventThresholds()[e];

  if (logFile.write((const uint8_t *)&header, sizeof(header)) != sizeof(header)) {
    logFile.close();
    storage::sessions().remove(logPath);
    Serial.println(F("ERROR: could not write the log header."));
    return false;
  }
  logFile.flush();

  // Free space is measured once: asking the filesystem walks all of it,
  // too slow to repeat while recording. From here on the bytes are counted.
  bytesLeft = freeBytes - LOG_RESERVE_BYTES;

  recording     = true;
  writeFailed   = false;
  pendingCount  = 0;
  samplesTaken  = 0;
  samplesStored = 0;
  for (int c = 0; c < CHANNEL_COUNT; c++) lastPeaks[c] = 0.0f;

  stats::reset();
  laps::resetLaps();
  records::clearNewFlags();
  startedAt = millis();
  lastFlush = millis();

  Serial.print(F("REC STARTED -> file: "));
  Serial.println(logPath);
  return true;
}

void stop() {
  recording = false;
  lastDurationMs = millis() - startedAt;
  Serial.println(F("REC STOPPED."));

  flush();
  writeFooter();
  logFile.close();

  records::commitSession(stats::sessionMax(), lastDurationMs / 60000);
  tracks::commitBestLap();
}

bool update() {
  if (!recording) return false;

  // Sample i belongs at i/rate seconds: counting from the start, rather
  // than waiting an interval after the previous one, keeps the time axis
  // true however irregular the loop is. If the loop was held up (a web
  // page), the samples it missed are filled in with the current values.
  uint32_t due = (uint32_t)((uint64_t)(millis() - startedAt) * rateHz / 1000) + 1;
  if (samplesTaken < due) {
    core::SessionSample sample = currentSample();
    while (samplesTaken < due) {
      if (pendingCount == PENDING_SAMPLES) flush();
      pending[pendingCount++] = sample;
      samplesTaken++;
    }
  }

  if (millis() - lastFlush >= LOG_FLUSH_MS) {
    lastFlush = millis();
    flush();
  }

  if (!writeFailed && bytesLeft > sizeof(pending)) return false;

  Serial.println(F("Storage full: recording stopped."));
  stop();
  return true;
}

bool isRecording() { return recording; }

const char *fileName() { return logPath; }

uint32_t minutesLogged() { return durationSeconds() / 60; }

uint32_t durationSeconds() {
  return (recording ? millis() - startedAt : lastDurationMs) / 1000;
}

}  // namespace recorder
