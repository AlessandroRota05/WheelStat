// WheelStat - the non-page endpoints of the website: live telemetry as JSON,
// session downloads, and every URL that changes something on the device.

#include "../Channels.h"
#include "../Gps.h"
#include "../LapTimer.h"
#include "../Motion.h"
#include "../Recorder.h"
#include "../Records.h"
#include "../SessionFormat.h"
#include "../Sessions.h"
#include "../Stats.h"
#include "../Tracks.h"
#include "../Weather.h"
#include "../core/Geo.h"
#include "../i18n/Text.h"
#include "WebInternal.h"

namespace web {

// ===========================================================================
// Live telemetry
// ===========================================================================

// Polled by the live page about three times a second. Sector arrays have
// one entry per sector of the current course (a single one without
// checkpoints).
void handleData() {
  const motion::State    &m        = motion::state();
  const weather::Reading &air      = weather::reading();
  const gps::Fix         &position = gps::fix();
  const laps::Course     &course   = laps::course();
  const int lapCount = laps::lapCount();
  const int sectors  = laps::sectorCount();

  PageWriter json(server, "application/json");

  json.printf("{\"lean\":%.1f,\"pitch\":%.1f,\"glat\":%.2f,\"glon\":%.2f,",
              m.lean, m.pitch, m.gLat, m.gLong);
  json.printf("\"temp\":%.1f,\"hum\":%.0f,\"risk\":%.0f,",
              air.temperature, air.humidity, air.gripRisk);
  json.printf("\"rec\":%d,\"min\":%lu,",
              recorder::isRecording() ? 1 : 0, (unsigned long)recorder::minutesLogged());
  json.printf("\"evw\":%u,\"evs\":%u,\"evl\":%u,\"evb\":%u,\"eva\":%u,",
              stats::eventCount(EV_WHEELIE), stats::eventCount(EV_STOPPIE),
              stats::eventCount(EV_LEAN), stats::eventCount(EV_HARD_BRAKE),
              stats::eventCount(EV_HARD_ACCEL));
  json.printf("\"gpsFix\":%d,\"gpsSpeed\":%.0f,\"gpsSat\":%u,\"gpsLat\":%.6f,\"gpsLon\":%.6f,",
              position.valid ? 1 : 0, position.speedKmh, (unsigned)position.satellites,
              position.lat, position.lon);
  json.printf("\"lapSet\":%d,\"lapNum\":%d,\"lapCur\":%lu,\"lapLast\":%lu,\"lapBest\":%lu,",
              course.finishSet ? 1 : 0, lapCount, (unsigned long)laps::currentLapMs(),
              (unsigned long)laps::lastLapMs(), (unsigned long)laps::bestLapMs());
  json.printf("\"finLat\":%.6f,\"finLon\":%.6f,", course.finish.lat, course.finish.lon);

  // Milliseconds behind (+) or ahead (-) of the reference lap, null when
  // there is nothing to compare with.
  int32_t delta;
  if (tracks::delta(delta)) json.printf("\"delta\":%ld,", (long)delta);
  else json.print(F("\"delta\":null,"));

  // Sectors of the last completed lap (zeros if there is none yet), and the
  // session best of each.
  json.print(F("\"sec\":["));
  for (int s = 0; s < sectors; s++)
    json.printf("%s%lu", s > 0 ? "," : "",
                (unsigned long)(lapCount > 0 ? laps::sectorTime(lapCount - 1, s) : 0));
  json.print(F("],\"secBest\":["));
  for (int s = 0; s < sectors; s++)
    json.printf("%s%lu", s > 0 ? "," : "", (unsigned long)laps::bestSectorMs(s));

  // The name was sanitised at creation: no quotes or backslashes in it.
  json.print(F("],\"trackName\":\""));
  json.print(tracks::hasActive() ? tracks::active().name : "");
  json.print(F("\"}"));
}

// ===========================================================================
// Session files
// ===========================================================================

bool requestedLog(char *path, size_t size) {
  if (sessions::resolvePath(server.arg("f").c_str(), path, size)) return true;
  server.send(404, CONTENT_TEXT, TR_E_FILE_NOT_FOUND);
  return false;
}

// Content-Disposition makes the browser download instead of showing text.
// "/LOG_12.BIN" + ".csv" -> "LOG_12.csv"
static void sendAttachmentHeader(const char *path, const char *extension) {
  char name[32];
  strlcpy(name, path + 1, sizeof(name));
  char *dot = strrchr(name, '.');
  if (dot != nullptr) *dot = '\0';
  strlcat(name, extension, sizeof(name));
  server.sendHeader("Content-Disposition", String("attachment; filename=") + name);
}

// Opens the requested log for one of the exports. On failure it has already
// answered. Logs left by old firmware are refused here: only the raw
// download knows what to do with them.
static bool openRequested(sessions::Opened &opened, char *path, size_t size) {
  if (rejectWhileRecording()) return false;
  if (!requestedLog(path, size)) return false;
  if (sessions::open(path, opened)) return true;
  server.send(400, CONTENT_TEXT, TR_E_NOT_A_SESSION);
  return false;
}

// Samples are read a few at a time: the whole log never sits in RAM.
constexpr int SAMPLES_PER_READ = 16;

template <typename Each>
static void forEachSample(sessions::Opened &opened, Each each) {
  core::SessionSample samples[SAMPLES_PER_READ];
  uint32_t index = 0;
  while (index < opened.sampleCount) {
    uint32_t wanted = opened.sampleCount - index;
    if (wanted > SAMPLES_PER_READ) wanted = SAMPLES_PER_READ;

    size_t bytes = opened.file.read((uint8_t *)samples, wanted * sizeof(core::SessionSample));
    uint32_t got = bytes / sizeof(core::SessionSample);
    if (got == 0) return;

    for (uint32_t i = 0; i < got; i++) each(samples[i], index + i);
    index += got;
  }
}

// The summary rows that follow the samples in the CSV export (see
// src/SessionFormat.h).
static void printCsvSummary(Print &csv, const core::SessionHeader &header,
                            const core::SessionFooter &footer) {
  using namespace sessionFormat;

  csv.print('\n');
  csv.print(ROW_TRACK);
  csv.print(',');
  csv.println(header.track);

  csv.print(ROW_SUMMARY);
  for (int c = 0; c < CHANNEL_COUNT; c++) {
    csv.print(',');
    csv.print(channelInfo(c).csvName);
  }
  csv.println(F(",Duration_s"));

  csv.print(ROW_MAX);
  for (int c = 0; c < CHANNEL_COUNT; c++) {
    csv.print(',');
    csv.print(footer.channelMax[c], channelInfo(c).decimals);
  }
  csv.print(',');
  csv.println(footer.durationMs / 1000);

  csv.print(ROW_EVENTS);
  for (int e = 0; e < EVENT_COUNT; e++) {
    const EventInfo &event = eventInfo(e);
    csv.print(',');
    csv.print(event.csvName);
    csv.print('>');
    if (event.inG) {
      csv.print(header.eventThreshold[e], 2);
      csv.print('G');
    } else {
      csv.print(header.eventThreshold[e], 0);
    }
  }
  csv.println();

  csv.print(ROW_TOTAL);
  for (int e = 0; e < EVENT_COUNT; e++) {
    csv.print(',');
    csv.print(footer.eventCount[e]);
  }
  csv.println();

  // Lap times, only if at least one lap was completed. Sector columns
  // appear only when the course had checkpoints.
  int lapCount = footer.lapCount > MAX_LAPS ? MAX_LAPS : footer.lapCount;
  if (lapCount == 0) return;
  int sectors = header.sectorCount > MAX_SECTORS ? MAX_SECTORS : header.sectorCount;

  csv.print('\n');
  csv.print(ROW_LAPS);
  csv.print(F(",Time_s"));
  if (sectors > 1)
    for (int s = 0; s < sectors; s++) csv.printf(",Sector_%d", s + 1);
  csv.println();

  for (int lap = 0; lap < lapCount; lap++) {
    csv.printf("%s%d,%.2f%s", LAP_PREFIX, lap + 1, footer.lapMs[lap] / 1000.0,
               lap == footer.bestLap ? "*" : "");
    if (sectors > 1)
      for (int s = 0; s < sectors; s++) csv.printf(",%.2f", footer.sectorMs[lap][s] / 1000.0);
    csv.println();
  }
}

// CSV export of a session. A log of the old text format is already a CSV:
// it goes out as it is.
void handleDownload() {
  if (rejectWhileRecording()) return;

  char path[32];
  if (!requestedLog(path, sizeof(path))) return;

  if (!sessions::isCurrentFormat(path + 1)) {
    File file = LittleFS.open(path, FILE_READ);
    if (!file) {
      server.send(500, CONTENT_TEXT, TR_E_READ_ERROR);
      return;
    }
    sendAttachmentHeader(path, ".csv");
    server.streamFile(file, "text/csv");
    file.close();
    return;
  }

  sessions::Opened opened;
  if (!sessions::open(path, opened)) {
    server.send(400, CONTENT_TEXT, TR_E_NOT_A_SESSION);
    return;
  }

  sendAttachmentHeader(path, ".csv");
  PageWriter csv(server, "text/csv");

  char line[core::CSV_LINE_SIZE];
  core::formatCsvHeader(line, sizeof(line));
  csv.println(line);

  const uint8_t rateHz = opened.header.rateHz;
  forEachSample(opened, [&](const core::SessionSample &sample, uint32_t index) {
    core::formatCsvRow(sample, index, rateHz, line, sizeof(line));
    csv.println(line);
  });

  // Static: 860 bytes are too many for the stack.
  static core::SessionFooter footer;
  if (sessions::readFooter(opened, footer)) printCsvSummary(csv, opened.header, footer);
  opened.file.close();
}

// The log as it is on flash, for the session viewer: the page decodes it in
// JavaScript, which costs the device nothing.
void handleSessionData() {
  sessions::Opened opened;
  char path[32];
  if (!openRequested(opened, path, sizeof(path))) return;

  opened.file.seek(0);
  server.streamFile(opened.file, "application/octet-stream");
  opened.file.close();
}

// Builds a GPX file (GPS Exchange Format) from the positions logged in a
// session: it opens in Google Earth or any software that reads GPS tracks,
// unlike the CSV which is a format of our own.
//
// No <time> in the points: the device has no clock synchronised to anything
// real (no RTC, no NTP: the access point is offline), so a made-up
// timestamp would mislead more than help. The file is valid all the same.
void handleGpx() {
  sessions::Opened opened;
  char path[32];
  if (!openRequested(opened, path, sizeof(path))) return;

  sendAttachmentHeader(path, ".gpx");
  PageWriter gpx(server, "application/gpx+xml");
  gpx.print(F("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              "<gpx version=\"1.1\" creator=\"WheelStat\" "
              "xmlns=\"http://www.topografix.com/GPX/1/1\">\n<trk><name>"));
  gpx.print(path + 1);
  gpx.print(F("</name><trkseg>\n"));

  forEachSample(opened, [&](const core::SessionSample &sample, uint32_t) {
    if ((sample.flags & core::SAMPLE_HAS_FIX) == 0) return;
    gpx.printf("<trkpt lat=\"%.7f\" lon=\"%.7f\"></trkpt>\n",
               core::fromE7(sample.latE7), core::fromE7(sample.lonE7));
  });
  opened.file.close();

  gpx.print(F("</trkseg></trk></gpx>\n"));
}

void handleDelete() {
  if (rejectWhileRecording()) return;

  char path[32];
  if (!requestedLog(path, sizeof(path))) return;

  bool removed = LittleFS.remove(path);
  Serial.print(removed ? F("Deleted ") : F("ERROR deleting "));
  Serial.println(path);

  redirect("/");
}

// ===========================================================================
// Records and tracks
// ===========================================================================

// With a track active this resets ONLY that track's history, not every
// saved track.
void handleRecordsReset() {
  records::reset();
  Serial.println(F("All-time records reset from the website."));
  redirect("/tracks");
}

void handleTrackCreate() {
  if (rejectWhileRecording()) return;
  // With no name or no fix nothing is created; the redirect brings the user
  // back to a page showing the current state either way.
  tracks::createHere(server.arg("name").c_str());
  redirect("/tracks");
}

void handleTrackSelect() {
  if (rejectWhileRecording()) return;
  tracks::select(server.arg("id").toInt());
  redirect("/tracks");
}

void handleTrackFree() {
  if (rejectWhileRecording()) return;
  tracks::select(tracks::NO_TRACK);
  redirect("/tracks");
}

void handleTrackDelete() {
  if (rejectWhileRecording()) return;
  tracks::remove(server.arg("id").toInt());
  redirect("/tracks");
}

void handleCheckpointAdd() {
  if (rejectWhileRecording()) return;
  tracks::addCheckpointHere();
  redirect("/tracks");
}

void handleCheckpointsClear() {
  if (rejectWhileRecording()) return;
  tracks::clearCheckpoints();
  redirect("/tracks");
}

}  // namespace web
