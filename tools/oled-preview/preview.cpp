// WheelStat - OLED preview on a PC.
//
// Compiles the real src/ui drawing code against the real Adafruit_GFX, with
// every other module replaced by the stand-ins below that return a made-up
// but plausible ride. Each screen is written as a PBM file (1 bit, 128x64)
// into the folder given on the command line; render.py turns them into the
// images used in the documentation. The build is in build.ps1.

#include <stdio.h>

#include <string>
#include <vector>

#include <qrcode.h>

#include "src/Buttons.h"
#include "src/Gps.h"
#include "src/LapTimer.h"
#include "src/Motion.h"
#include "src/Power.h"
#include "src/Recorder.h"
#include "src/Records.h"
#include "src/Settings.h"
#include "src/Stats.h"
#include "src/Storage.h"
#include "src/Tracks.h"
#include "src/Version.h"
#include "src/Weather.h"
#include "src/drivers/Display.h"
#include "src/i18n/Text.h"
#include "src/ui/Oled.h"
#include "src/web/Web.h"

// ===========================================================================
// Fake time and the panel
// ===========================================================================

static uint32_t fakeNow = 0;
uint32_t millis() { return fakeNow; }
void delay(uint32_t ms) { fakeNow += ms; }

DisplayDriver display;
bool displayBegin() { return true; }
const char *displayName() { return "SSD1306"; }

typedef std::vector<uint8_t> Frame;
static std::vector<Frame> frames;  // every display() since the last clear

void Adafruit_SSD1306::display() {
  frames.push_back(Frame(buffer_, buffer_ + sizeof(buffer_)));
}

// ===========================================================================
// The ride the stand-ins describe
// ===========================================================================

static motion::State     motionState;
static weather::Reading  air;
static gps::Fix          position;
static laps::Course      lapCourse;
static tracks::Track     track;
static records::AllTime  allTime;
static core::Settings    config;
static float             sessionBest[CHANNEL_COUNT];
static uint16_t          events[EVENT_COUNT];
static bool              newRecord[CHANNEL_COUNT];

static bool     hasTrack = false, lapOn = false, hasDelta = false;
static bool     recording = false, wifiOn = false, keys = false, powerSense = false;
static std::string outDirForQr;
static int32_t  deltaMs = 0;
static uint32_t lapMs = 0, lastMs = 0, bestMs = 0;
static int      lapsDone = 0;

namespace motion {
const State &state() { return motionState; }
}
namespace weather {
const Reading &reading() { return air; }
}
namespace gps {
const Fix &fix() { return position; }
float distanceTo(double, double) { return 412; }
}
namespace laps {
const Course &course() { return lapCourse; }
bool nextTarget(double &lat, double &lon) { lat = lon = 0; return true; }
bool lapRunning() { return lapOn; }
uint32_t currentLapMs() { return lapMs; }
int lapCount() { return lapsDone; }
int bestLapIndex() { return bestMs ? 0 : -1; }
uint32_t bestLapMs() { return bestMs; }
uint32_t lastLapMs() { return lastMs; }
}
namespace tracks {
bool hasActive() { return hasTrack; }
const Track &active() { return track; }
bool delta(int32_t &ms) { ms = deltaMs; return hasDelta; }
}
namespace records {
const AllTime &current() { return allTime; }
bool isNew(uint8_t channel) { return newRecord[channel]; }
bool anyNew() {
  for (bool n : newRecord) if (n) return true;
  return false;
}
}
namespace recorder {
bool isRecording() { return recording; }
const char *fileName() { return "/LOG_7.BIN"; }
uint32_t minutesLogged() { return 23; }
uint32_t durationSeconds() { return 1384; }
}
namespace settings {
uint8_t logRateHz() { return config.logRateHz; }
const char *wifiName() { return config.wifiName; }
const char *wifiPassword() { return config.wifiPassword; }
float leanAlertDeg() { return config.leanAlertDeg; }
}
namespace storage {
bool ok() { return true; }
uint64_t freeBytes() { return 1184 * 1024; }
uint64_t totalBytes() { return 1408 * 1024; }
uint64_t usedBytes() { return totalBytes() - freeBytes(); }
bool onCard() { return false; }
}
namespace power {
bool sensing() { return powerSense; }
float volts() { return 13.8f; }
}

// esp_qrcode on a PC: qr_matrix.py encodes, this hands the matrix over in
// the core's format (a handle the module getter understands).
static std::vector<uint8_t> qrMatrix;  // size, then size*size cells
int esp_qrcode_generate(esp_qrcode_config_t *cfg, const char *text) {
  const char *python = getenv("WHEELSTAT_PYTHON");
  const char *script = getenv("WHEELSTAT_QR_SCRIPT");
  if (!python || !script) return -1;
  std::string input = outDirForQr + "/qr_input.txt";
  FILE *f = fopen(input.c_str(), "wb");
  fputs(text, f);
  fclose(f);
  std::string command = std::string("\"\"") + python + "\" \"" + script + "\" \"" + input + "\"\"";
  FILE *out = popen(command.c_str(), "r");
  if (!out) return -1;
  int size = 0;
  if (fscanf(out, "%d", &size) != 1) { pclose(out); return -1; }
  qrMatrix.assign(1 + size * size, 0);
  qrMatrix[0] = size;
  char row[256];
  for (int y = 0; y < size && fscanf(out, "%255s", row) == 1; y++)
    for (int x = 0; x < size; x++) qrMatrix[1 + y * size + x] = row[x] == '1';
  pclose(out);
  cfg->display_func(qrMatrix.data());
  return 0;
}
int esp_qrcode_get_size(esp_qrcode_handle_t code) { return code[0]; }
bool esp_qrcode_get_module(esp_qrcode_handle_t code, int x, int y) {
  return code[1 + y * code[0] + x] != 0;
}

namespace stats {
const float *sessionMax() { return sessionBest; }
uint16_t eventCount(uint8_t event) { return events[event]; }
}
namespace web {
bool isActive() { return wifiOn; }
IPAddress address() { return IPAddress(192, 168, 4, 1); }
int clientCount() { return 1; }
void handleClient() {}
}
namespace buttons {
bool anyPressed() { return keys; }
void holdOff() {}
}

// ===========================================================================
// Output
// ===========================================================================

static std::string outDir;

static void writeFrame(const Frame &frame, const std::string &name) {
  std::string path = outDir + "/" + name + ".pbm";
  FILE *out = fopen(path.c_str(), "wb");
  if (!out) { perror(path.c_str()); exit(1); }
  fprintf(out, "P1\n128 64\n");
  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 128; x++)
      fputc((frame[x + (y / 8) * 128] >> (y & 7)) & 1 ? '1' : '0', out);
    fputc('\n', out);
  }
  fclose(out);
}

// The last frame sent to the panel.
static void save(const std::string &name) { writeFrame(frames.back(), name); }

// Every frame since the last call, numbered: an animation for render.py.
static void saveSequence(const std::string &name) {
  for (size_t i = 0; i < frames.size(); i++) {
    char suffix[16];
    snprintf(suffix, sizeof(suffix), "_%03u", (unsigned)i);
    writeFrame(frames[i], name + suffix);
  }
  frames.clear();
}

static void goTo(oled::Page target) {
  while (oled::currentPage() != target) oled::changePage(+1);
}

static void showPage(oled::Page target, const char *name) {
  goTo(target);
  fakeNow = 0;  // blinking things (REC, the grip alert) in their "on" phase
  oled::render();
  save(name);
}

// ===========================================================================
// The ride
// ===========================================================================

static void setUpRide() {
  config = core::Settings{};
  strlcpy(config.wifiName, "WheelStat", sizeof(config.wifiName));
  strlcpy(config.wifiPassword, "k7m2x9qa", sizeof(config.wifiPassword));
  config.leanAlertDeg = 45;
  config.logRateHz = 3;

  motionState = {34.0f, 2.0f, 34.0f, 0, 0, 0.62f, -0.35f};
  air = {true, 18.4f, 62, 15};
  position = {true, true, 45.53412, 10.08321, 87, 9};

  lapCourse.finishSet = true;
  hasTrack = true;
  strlcpy(track.name, "Franciacorta", sizeof(track.name));
  lapOn = true;
  lapMs = 83000;
  lapsDone = 5;
  lastMs = 84320;
  bestMs = 82810;
  hasDelta = true;
  deltaMs = -420;

  float best[CHANNEL_COUNT] = {52, 49, 21, 9, 1.12f, 1.05f, 0.58f, 1.04f, 168};
  memcpy(allTime.channels, best, sizeof(best));
  float session[CHANNEL_COUNT] = {48, 51, 18, 6, 0.98f, 1.07f, 0.52f, 0.96f, 171};
  memcpy(sessionBest, session, sizeof(session));
  newRecord[CH_LEAN_LEFT] = newRecord[CH_SPEED] = true;
  uint16_t counts[EVENT_COUNT] = {3, 0, 17, 9, 12};
  memcpy(events, counts, sizeof(counts));

  recording = true;
  wifiOn = true;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: preview <output folder>\n");
    return 1;
  }
  outDir = argv[1];
  outDirForQr = outDir;
  setUpRide();
  oled::begin();

  // --- Boot ----------------------------------------------------------------
  oled::splashIntro();
  saveSequence("anim_boot");
  oled::splashIntro();
  save("boot_logo");

  const char *labels[] = {"OLED", TR_O_BOOT_WEATHER, "IMU", "GPS", TR_O_BOOT_STORAGE};
  const char *names[]  = {"SSD1306", "DHT22", "BNO055", "u-blox M8", "LittleFS"};
  oled::checklistBegin();
  for (int row = 0; row < 5; row++) {
    oled::checklistPending(row, labels[row], names[row]);
    oled::checklistResult(row, true, names[row]);
  }
  save("boot_checklist");

  fakeNow = 2 * 90;  // the dot on the right flank of the upper loop
  oled::drawCalibration(1);
  save("boot_calibration");
  frames.clear();

  // --- Live pages ----------------------------------------------------------
  showPage(oled::Page::Lean,     "page_0_lean");
  showPage(oled::Page::Weather,  "page_1_weather");
  showPage(oled::Page::GForce,   "page_2_gforce");
  motionState.wheelie = 18;
  showPage(oled::Page::Pitch,    "page_3_wheelie");
  showPage(oled::Page::Gps,      "page_4_gps");
  showPage(oled::Page::Laps,     "page_5_laps");
  showPage(oled::Page::Records1, "page_6_records1");
  showPage(oled::Page::Records2, "page_7_records2");
  showPage(oled::Page::Storage,  "page_8_storage");
  showPage(oled::Page::WiFi,     "page_9_wifi");
  showPage(oled::Page::WiFiQr,   "page_10_qr");

  // Variants worth a picture of their own.
  motionState.lean = -47; motionState.leanAbs = 47;
  air.gripRisk = 40;
  showPage(oled::Page::Lean, "page_0_lean_alert");
  motionState.lean = 34; motionState.leanAbs = 34;
  air.gripRisk = 15;

  hasDelta = true; deltaMs = 650;
  showPage(oled::Page::Laps, "page_5_laps_behind");
  deltaMs = -420;

  position.valid = false; position.satellites = 3;
  showPage(oled::Page::Gps, "page_4_gps_nofix");
  position.valid = true; position.satellites = 9;

  showPage(oled::Page::Laps, "popup_base");
  oled::popup(TR_O_POPUP_FINISH_OK);
  save("popup_finish");

  // --- Navigation: every page in turn, with the slide between them -------
  goTo(oled::Page::Lean);
  fakeNow = 0;
  oled::render();
  frames.clear();
  oled::render();
  for (int i = 0; i < (int)oled::Page::Count; i++) {
    fakeNow = 0;
    oled::changePage(+1);
  }
  saveSequence("anim_pages");

  // --- End of session ------------------------------------------------------
  keys = true;
  oled::showSessionSummary();
  writeFrame(frames[0], "summary_1");
  // The first frame after each key press is the next page.
  writeFrame(frames[1], "summary_2");
  writeFrame(frames[2], "summary_3");
  frames.clear();

  puts("ok");
  return 0;
}
