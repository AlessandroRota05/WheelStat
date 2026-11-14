#include "App.h"

#include <Wire.h>

#include "../Config.h"
#include "Buttons.h"
#include "Channels.h"
#include "Gps.h"
#include "LapTimer.h"
#include "Motion.h"
#include "Recorder.h"
#include "Settings.h"
#include "Stats.h"
#include "Storage.h"
#include "Tracks.h"
#include "Tuning.h"
#include "Util.h"
#include "Version.h"
#include "Weather.h"
#include "drivers/Display.h"
#include "drivers/GpsModule.h"
#include "drivers/Imu.h"
#include "drivers/WeatherSensor.h"
#include "i18n/Text.h"
#include "ui/Oled.h"
#include "web/Web.h"

namespace app {

// ===========================================================================
// Boot
// ===========================================================================

// One row of the boot checklist: the name comes from the driver itself, so
// the screen always shows the configuration that was really compiled, and
// the outcome is a real check - an unplugged component gets a cross.
struct BootStep {
  const char *label;          // on the OLED checklist
  const char *(*driverName)();
  bool (*init)();
  const char *serialLabel;    // padded so the outcomes line up
  const char *failureHint;
};

static const char *storageName() { return "LittleFS"; }

// The GPS tick means "the module sends NMEA", NOT "fix acquired": the fix
// comes tens of seconds later and has its own page.
//
// The weather sensor comes after Wire.begin(): an I2C driver talks on the
// bus straight away.
static const BootStep BOOT_STEPS[] = {
  { "OLED",            displayName,         oled::begin,    "Boot OLED...... ",
    "ERROR! Check SDA/SCL." },
  { TR_O_BOOT_WEATHER, weatherSensor::name, weather::begin, "Boot weather... ",
    "ERROR! Weather sensor not found." },
  { "IMU",             imu::name,           motion::begin,  "Boot IMU....... ",
    "ERROR! IMU not found (try address 0x29)." },
  { "GPS",             gpsModule::name,     gps::begin,     "Boot GPS....... ",
    "MISSING! No NMEA sentence: module unplugged, unpowered, or TX/RX swapped." },
  { TR_O_BOOT_STORAGE, storageName,         storage::begin, "Boot storage... ",
    "ERROR! Flash partition cannot be mounted." },
};
constexpr int BOOT_STEP_COUNT = sizeof(BOOT_STEPS) / sizeof(BOOT_STEPS[0]);

static void runBootSteps() {
  for (int row = 0; row < BOOT_STEP_COUNT; row++) {
    const BootStep &step = BOOT_STEPS[row];
    Serial.print(step.serialLabel);

    // The display is the only one that cannot narrate its own power-up:
    // until it is on there is nothing to write on. Its row is drawn after
    // the splash, once the result is known.
    bool ok;
    if (row == 0) {
      ok = step.init();
      oled::splashIntro();
      oled::checklistBegin();
      oled::checklistPending(row, step.label, step.driverName());
    } else {
      oled::checklistPending(row, step.label, step.driverName());
      ok = step.init();
    }
    oled::checklistResult(row, ok);

    Serial.println(ok ? "OK." : step.failureHint);
  }

  if (storage::ok())
    Serial.printf("Storage: %u KB free.\n", (unsigned)(storage::freeBytes() / 1024));
}

// Blocks until the magnetometer is ready; OK skips it. Which quantity
// decides is up to the IMU driver: a chip that calibrates itself reports 3
// at once and this screen closes by itself.
static void waitImuCalibration() {
  if (!motion::available() || !oled::available()) return;

  bool calibrated = false;
  uint32_t lastDraw = 0;

  while (true) {
    uint8_t level = motion::calibrationLevel();
    if (level >= 3) {
      calibrated = true;
      break;
    }
    if (buttons::okPressed()) break;

    if (elapsed(lastDraw, DISPLAY_INTERVAL_MS)) oled::drawCalibration(level);
    delay(LOOP_PAUSE_MS);
  }

  // No events or maxima inherited from the calibration movements.
  stats::reset();
  laps::resetLaps();

  if (calibrated) {
    Serial.println(F("Magnetometer calibrated (MAG=3)."));
    oled::drawCalibrationDone();
    oled::waitAnyKey();
  } else {
    Serial.println(F("Calibration skipped by the user."));
  }

  // The key pressed here must not also drive the menu right after.
  buttons::holdOff();
}

void setup() {
  Serial.begin(115200);
  delay(500);  // let the supply settle before talking to the sensors

  Serial.println(F("=== WHEELSTAT BOOT ==="));
  Serial.print(F("Firmware: "));
  Serial.println(FIRMWARE_VERSION);

  buttons::begin();

  // I2C bus shared by OLED, IMU and (if I2C) the weather sensor.
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_CLOCK_HZ);

  runBootSteps();

  settings::load();  // needs the storage the last boot step mounted
  tracks::begin();   // free mode, with its all-time records
  web::begin();      // routes only: the access point starts from its page
  web::onFirmwareUpload([]() { oled::notice(TR_O_UPDATING); });

  Serial.println(F("=== SYSTEM READY ==="));

  delay(SPLASH_HOLD_MS);  // time to read the checklist
  waitImuCalibration();
}

// ===========================================================================
// Buttons
// ===========================================================================

// Do it with the bike upright and level. A blocking popup is fine here: the
// zero is always set while stationary.
static void zeroCalibration() {
  if (!motion::zeroHere()) return;
  oled::popup(TR_O_POPUP_ZERO_OK);
  Serial.println(F("Zero calibration done."));
}

// To be done standing on the start/finish line with the fix already locked.
static void setFinishLine() {
  if (!tracks::setFinishLineHere()) {
    oled::popup(TR_O_POPUP_NO_FIX);
    Serial.println(F("Cannot set the finish line: no GPS fix."));
    return;
  }
  oled::popup(TR_O_POPUP_FINISH_OK);
  Serial.println(F("Finish line set on the current position."));
}

// OK does a different job on each page; where it is not listed it does
// nothing on purpose (see the records pages in OledPages.cpp).
static void handleOk() {
  switch (oled::currentPage()) {
    case oled::Page::Lean:
    case oled::Page::Pitch:
      zeroCalibration();
      break;
    case oled::Page::Laps:
      setFinishLine();
      break;
    case oled::Page::WiFi:
      if (web::isActive()) web::stop();
      else web::start();
      break;
    default:
      break;
  }
}

static void toggleRecording() {
  if (!recorder::isRecording()) {
    recorder::start();
    return;
  }
  // The records are updated BEFORE the summary is drawn, so the "new
  // record" asterisks are ready when the page appears.
  recorder::stop();
  oled::showSessionSummary();
}

static void handleButtons() {
  using buttons::Button;

  switch (buttons::poll()) {
    case Button::None: return;
    case Button::Up:   oled::changePage(-1); break;
    case Button::Down: oled::changePage(+1); break;
    case Button::Ok:   handleOk();           break;
    case Button::Log:  toggleRecording();    break;
  }

  // Re-armed AFTER the action: holding the key during a page transition
  // must not queue presses on top of the animation.
  buttons::holdOff();
}

// ===========================================================================
// Serial telemetry
// ===========================================================================

// Without this, debugging on the bench is impossible.
static void printTelemetry() {
  const motion::State    &m        = motion::state();
  const weather::Reading &air      = weather::reading();
  const gps::Fix         &position = gps::fix();

  Serial.println(F("---- LIVE TELEMETRY ----"));
  Serial.printf("Lean     : %.2f deg\n", m.leanAbs);
  Serial.printf("Wheelie  : %.2f deg | Stoppie: %.2f deg\n", m.wheelie, m.stoppie);
  Serial.printf("G_Lat    : %.2f | G_Lon: %.2f\n", m.gLat, m.gLong);
  Serial.printf("Weather  : %.2f C / %.2f %%\n", air.temperature, air.humidity);

  if (position.valid) {
    Serial.printf("GPS      : fix OK, %.0f km/h, %u sat, %.5f, %.5f\n", position.speedKmh,
                  (unsigned)position.satellites, position.lat, position.lon);
  } else {
    Serial.printf("GPS      : NO FIX (%u sat locked)\n", (unsigned)position.satellites);
  }

  if (!laps::course().finishSet) {
    Serial.println(F("Laps     : finish line not set"));
  } else if (laps::bestLapIndex() >= 0) {
    Serial.printf("Laps     : %d completed, best %.2fs\n", laps::lapCount(),
                  laps::bestLapMs() / 1000.0);
  } else {
    Serial.printf("Laps     : %d completed\n", laps::lapCount());
  }

  Serial.print(F("Events   : "));
  for (int e = 0; e < EVENT_COUNT; e++)
    Serial.printf("%s=%u ", eventInfo(e).csvName, (unsigned)stats::eventCount(e));
  Serial.println();

  if (motion::available()) motion::printCalibration(Serial);
  else Serial.println(F("Calibr.  : IMU MISSING"));
  Serial.println(F("------------------------"));
}

// ===========================================================================
// Main loop
// ===========================================================================

// Cooperative scheduling: every job has its own timer and runs only when
// due, so the IMU is read at the highest rate and no job blocks the others.
void loop() {
  static uint32_t lastWeather = 0;
  static uint32_t lastDisplay = 0;
  static uint32_t lastSerial  = 0;

  if (elapsed(lastWeather, WEATHER_INTERVAL_MS)) weather::update();

  // IMU and GPS every pass: not to miss peaks, and because NMEA bytes
  // arrive as a stream that must be drained at once.
  if (motion::update()) stats::sample();
  gps::update();
  tracks::onLapEvent(laps::update());  // uses the position just read

  handleButtons();
  web::handleClient();

  if (elapsed(lastDisplay, DISPLAY_INTERVAL_MS)) oled::render();
  if (elapsed(lastSerial, SERIAL_INTERVAL_MS))   printTelemetry();

  // The recorder stops by itself when the storage runs out: say why, then
  // show the summary like after any other recording.
  if (recorder::update()) {
    oled::popup(TR_O_POPUP_STORAGE_FULL);
    oled::showSessionSummary();
  }

  delay(LOOP_PAUSE_MS);
}

}  // namespace app
