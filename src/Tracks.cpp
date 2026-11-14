#include "Tracks.h"

#include "Gps.h"
#include "Records.h"
#include "Storage.h"

namespace tracks {

// "WST5": bump it whenever the Track struct changes.
constexpr uint32_t TRACK_MAGIC = 0x57535435;

// Does not start with "LOG_", so the session download/delete endpoints can
// never touch it.
constexpr char FREE_RECORDS_PATH[] = "/RECORD.BIN";

// Room for the longest path with any int id.
constexpr size_t PATH_SIZE = 32;

// A trace with fewer points than this is not worth keeping.
constexpr int MIN_REFERENCE_POINTS = 4;

static int   activeTrackId = NO_TRACK;
static Track activeTrack   = {};

static core::DeltaTracker deltaTracker;
static bool     deltaValid = false;
static int32_t  deltaMs    = 0;
static uint32_t reportsSeen = 0;

static void definitionPath(int id, char *out, size_t size) {
  snprintf(out, size, "/TRACK_%d.BIN", id);
}

static void recordsPath(int id, char *out, size_t size) {
  snprintf(out, size, "/TRACK_%d_REC.BIN", id);
}

static bool save(int id, Track &track) {
  char path[PATH_SIZE];
  definitionPath(id, path, sizeof(path));
  if (storage::saveStruct(path, track, TRACK_MAGIC)) return true;
  Serial.println(F("ERROR: could not save the track."));
  return false;
}

static void saveActive() {
  if (hasActive()) save(activeTrackId, activeTrack);
}

// Ids are tried in order and never compacted, so a deleted id is only
// reused by a later creation - never silently renumbered.
static int firstFreeId() {
  char path[PATH_SIZE];
  for (int id = 1; id <= MAX_TRACKS; id++) {
    definitionPath(id, path, sizeof(path));
    if (!LittleFS.exists(path)) return id;
  }
  return NO_TRACK;
}

// Removes the characters that would break the /data JSON or the CSV summary
// (quotes, backslash, comma, line breaks).
static void sanitizeName(char *name) {
  for (int i = 0; name[i] != '\0'; i++) {
    char c = name[i];
    if (c == '"' || c == '\\' || c == ',' || c == '\n' || c == '\r') name[i] = '_';
  }
}

// A gate on the current position. Its direction is learned from the first
// pass (see core/Gate.h): the rider is usually standing still here.
static laps::Gate gateHere() {
  const gps::Fix &position = gps::fix();
  return { position.lat, position.lon, 0.0f, core::GATE_UNKNOWN };
}

static void dropReference() {
  activeTrack.referenceLapMs  = 0;
  activeTrack.referencePoints = 0;
  deltaValid = false;
}

void begin() {
  records::open(FREE_RECORDS_PATH);
}

int activeId() { return activeTrackId; }

bool hasActive() { return activeTrackId != NO_TRACK; }

const Track &active() { return activeTrack; }

bool load(int id, Track &out) {
  char path[PATH_SIZE];
  definitionPath(id, path, sizeof(path));
  return storage::loadStruct(path, out, TRACK_MAGIC);
}

bool select(int id) {
  char path[PATH_SIZE];

  if (id < 0) {
    activeTrackId = NO_TRACK;
    activeTrack   = Track{};
    strlcpy(path, FREE_RECORDS_PATH, sizeof(path));
  } else {
    if (!load(id, activeTrack)) {
      Serial.println(F("ERROR: track not found or invalid."));
      return false;
    }
    activeTrackId = id;
    recordsPath(id, path, sizeof(path));
  }

  laps::setCourse(activeTrack.course);
  laps::resetLaps();
  deltaValid = false;
  records::open(path);

  Serial.print(F("Active track: "));
  Serial.println(hasActive() ? activeTrack.name : "none (free mode)");
  return true;
}

int createHere(const char *name) {
  if (name == nullptr || name[0] == '\0' || !gps::fix().valid) return NO_TRACK;

  int id = firstFreeId();
  if (id == NO_TRACK) {
    Serial.println(F("ERROR: no free slot for a new track."));
    return NO_TRACK;
  }

  // Static: a Track is about 2 KB, too much for the stack of a web handler.
  static Track track;
  track = Track{};
  strlcpy(track.name, name, sizeof(track.name));
  sanitizeName(track.name);
  track.course.finishSet = true;
  track.course.finish    = gateHere();
  if (!save(id, track)) return NO_TRACK;

  Serial.print(F("Track created: "));
  Serial.println(track.name);

  select(id);  // activated straight away: the most convenient flow
  return id;
}

void remove(int id) {
  if (id <= 0) return;

  char path[PATH_SIZE];
  definitionPath(id, path, sizeof(path));
  LittleFS.remove(path);
  recordsPath(id, path, sizeof(path));
  LittleFS.remove(path);

  if (id == activeTrackId) select(NO_TRACK);
  Serial.printf("Track deleted: %d\n", id);
}

bool setFinishLineHere() {
  if (!gps::fix().valid) return false;

  laps::Course course = laps::course();
  course.finishSet = true;
  course.finish    = gateHere();
  laps::setCourse(course);

  // New line, new count: the laps already done were on another one.
  laps::resetLaps();
  dropReference();

  // With a saved track selected the new position becomes permanent for it
  // (handy to refine the line while standing there).
  activeTrack.course = course;
  saveActive();
  return true;
}

bool addCheckpointHere() {
  laps::Course &course = activeTrack.course;
  if (!hasActive() || !gps::fix().valid || course.checkpointCount >= MAX_CHECKPOINTS)
    return false;

  course.checkpoints[course.checkpointCount++] = gateHere();
  saveActive();
  laps::setCourse(course);

  Serial.printf("Checkpoint added, total: %u\n", (unsigned)course.checkpointCount);
  return true;
}

void clearCheckpoints() {
  if (!hasActive()) return;

  activeTrack.course.checkpointCount = 0;
  saveActive();
  laps::setCourse(activeTrack.course);
  Serial.println(F("Checkpoints removed from the active track."));
}

// The lap just closed becomes the reference if there is none or it is
// faster. Laps past MAX_LAPS are not timed, so they cannot compete.
static void considerReference() {
  if (laps::lapCount() == 0 || laps::lapCount() >= MAX_LAPS) return;

  uint32_t lapMs = laps::lastLapMs();
  if (activeTrack.referencePoints > 0 && lapMs >= activeTrack.referenceLapMs) return;

  const core::TracePoint *points;
  int count = laps::closedLapTrace(points);
  if (count < MIN_REFERENCE_POINTS) return;

  for (int i = 0; i < count; i++) activeTrack.reference[i] = points[i];
  activeTrack.referencePoints = (uint16_t)count;
  activeTrack.referenceLapMs  = lapMs;
  saveActive();
  Serial.printf("Reference lap updated (%d points).\n", count);
}

// Once per GPS report, with the lap time OF THAT REPORT: pairing the last
// position with "now" would add the age of the position to the delta.
static void refreshDelta() {
  uint32_t reports = laps::reportCount();
  if (reports == reportsSeen) return;
  reportsSeen = reports;

  double   lat, lon;
  uint32_t lapMs;
  deltaValid = laps::lastPosition(lat, lon, lapMs) &&
               deltaTracker.compute(activeTrack.reference, activeTrack.referencePoints,
                                    lat, lon, lapMs, deltaMs);
}

void onLapEvent(laps::Event event) {
  // A pass taught a gate its direction: keep it, so the next sessions do
  // not have to learn it again. (In free mode the course lives in RAM.)
  if (laps::takeCourseChanged()) {
    activeTrack.course = laps::course();
    saveActive();
  }

  if (event == laps::Event::Lap || event == laps::Event::BestLap) considerReference();

  // Every pass over the finish line starts a lap: the delta search goes
  // back to the beginning of the reference.
  if (event != laps::Event::None && event != laps::Event::Sector) deltaTracker.restart();

  refreshDelta();
}

bool delta(int32_t &ms) {
  ms = deltaMs;
  return deltaValid && laps::lapRunning() && gps::fix().valid;
}

bool commitBestLap() {
  // In free mode there is nothing to compare with: the best lap stays a
  // session best and is lost at the next recording.
  if (!hasActive() || laps::bestLapIndex() < 0) return false;

  uint32_t best = laps::bestLapMs();
  if (activeTrack.bestLapMs != 0 && best >= activeTrack.bestLapMs) return false;

  activeTrack.bestLapMs = best;
  saveActive();
  Serial.println(F("New lap record for this track."));
  return true;
}

}  // namespace tracks
