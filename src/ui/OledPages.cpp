// WheelStat - OLED live pages, status bar, page transition and popup.

#include "Oled.h"

#include "../Gps.h"
#include "../LapTimer.h"
#include "../Motion.h"
#include "../Power.h"
#include "../Recorder.h"
#include "../Records.h"
#include "../Settings.h"
#include "../Stats.h"
#include "../Storage.h"
#include "../Tracks.h"
#include "../Tuning.h"
#include "../Util.h"
#include "../Weather.h"
#include "../i18n/Text.h"
#include "../web/Web.h"
#include "OledDraw.h"

#include <qrcode.h>

namespace oled {

static Page page = Page::Lean;

constexpr int PAGE_COUNT = (int)Page::Count;

// ===========================================================================
// Status bar
// ===========================================================================

// Icons, one row per byte pair, most significant bit on the left.
static const uint8_t ICON_FLAG[] PROGMEM = {  // 7x8: chequered flag on a pole
  0xD4, 0xAA, 0xD4, 0xAA, 0x80, 0x80, 0x80, 0x80
};
static const uint8_t ICON_WIFI[] PROGMEM = {  // 9x7: two arcs and a dot
  0x3E, 0x00, 0x41, 0x00, 0x80, 0x80, 0x1C, 0x00, 0x22, 0x00, 0x00, 0x00, 0x08, 0x00
};

// GPS: a ring, with a dot in the middle once there is a fix - the same
// symbol as a map's "you are here". Followed by the satellites in view, or
// "--" when the module never answered.
static int drawGpsIcon(int x) {
  const gps::Fix &position = gps::fix();
  display.drawCircle(x + 3, 5, 3, COLOR_OFF);
  if (position.valid) display.fillCircle(x + 3, 5, 1, COLOR_OFF);
  display.setCursor(x + 9, 2);
  if (position.modulePresent) display.print(position.satellites);
  else display.print(F("--"));
  return display.getCursorX() + 5;
}

// The right end: while recording, a blinking dot and how long it has been
// going (m:ss, then h:mm past 100 minutes); otherwise the bike's voltage
// when it is read, or nothing.
static void drawRightStatus() {
  char text[24] = "";  // room for any uint32: keeps -Wformat-truncation quiet
  bool recording = recorder::isRecording();
  if (recording) {
    uint32_t s = recorder::durationSeconds();
    if (s < 6000) snprintf(text, sizeof(text), "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
    else snprintf(text, sizeof(text), "%luh%02lu", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60));
  } else if (power::sensing()) {
    snprintf(text, sizeof(text), "%.1fV", power::volts());
  }

  int x = SCREEN_WIDTH - 2 - strlen(text) * FONT_CHAR_WIDTH;
  display.setCursor(x, 2);
  display.print(text);

  // It blinks: recording must be visible out of the corner of the eye.
  if (recording && (millis() / REC_BLINK_MS) % 2 == 0) display.fillCircle(x - 6, 5, 2, COLOR_OFF);
}

// Inverted bar on every live page: what is working, at a glance, from any
// page. GPS and satellites, the flag when a finish line is set (laps are
// being counted), the WiFi when the network is on, and the recording.
void drawStatusBar() {
  display.fillRect(0, 0, SCREEN_WIDTH, 11, COLOR_ON);
  display.setTextSize(1);
  display.setTextColor(COLOR_OFF);

  int x = drawGpsIcon(2);
  if (laps::course().finishSet) {
    display.drawBitmap(x, 2, ICON_FLAG, 7, 8, COLOR_OFF);
    x += 12;
  }
  if (web::isActive()) display.drawBitmap(x, 2, ICON_WIFI, 9, 7, COLOR_OFF);

  drawRightStatus();
  display.setTextColor(COLOR_ON);
}

// ===========================================================================
// Pages
// ===========================================================================

// Bar under the big number: it fills from the zero mark towards the side
// the bike is leaning, and the two notches above it are where the alert
// starts - they move inwards as the grip risk rises, so the margin left can
// be seen, not just the angle.
static void drawLeanBar(float signedLean, float safeLean) {
  const int BAR_MARGIN = 4;
  const int CENTER     = SCREEN_WIDTH / 2;
  const int HALF_BAR   = CENTER - BAR_MARGIN - 2;  // usable reach on each side
  const int FULL_SCALE = 60;                       // degrees at the end of the bar

  display.drawRoundRect(BAR_MARGIN, 56, SCREEN_WIDTH - 2 * BAR_MARGIN, 6, 2, COLOR_ON);
  display.drawFastVLine(CENTER, 54, 10, COLOR_ON);  // zero mark

  int notch = constrain(map((int)safeLean, 0, FULL_SCALE, 0, HALF_BAR), 0, HALF_BAR);
  display.drawFastVLine(CENTER - notch, 53, 3, COLOR_ON);
  display.drawFastVLine(CENTER + notch, 53, 3, COLOR_ON);

  int reach = constrain(map((int)fabsf(signedLean), 0, FULL_SCALE, 0, HALF_BAR), 0, HALF_BAR - 1);
  if (signedLean >= 0) display.fillRect(CENTER + 1, 58, reach, 2, COLOR_ON);
  else                 display.fillRect(CENTER - reach, 58, reach, 2, COLOR_ON);
}

// Solid triangle beside the number pointing to the side of the lean: the
// number alone is always positive. Hidden near zero, where it would flicker.
static void drawLeanSide(float signedLean) {
  if (fabsf(signedLean) < 2.0f) return;
  const int X = 92, Y = 37;
  if (signedLean > 0) display.fillTriangle(X, Y - 6, X, Y + 6, X + 8, Y, COLOR_ON);
  else                display.fillTriangle(X + 8, Y - 6, X + 8, Y + 6, X, Y, COLOR_ON);
}

// The session's maximum on each side, small, at the ends of the bar it
// belongs to: how far it went, next to how far it is now. Session means
// since the recording started (or since boot when not recording).
static void drawLeanMaxima() {
  const float *best = stats::sessionMax();
  char right[4];
  snprintf(right, sizeof(right), "%d", constrain((int)best[CH_LEAN_RIGHT], 0, 99));

  display.setCursor(2, 33);
  display.print(TR_O_LBL_LEFT);
  display.setCursor(2, 43);
  display.print(constrain((int)best[CH_LEAN_LEFT], 0, 99));

  display.setCursor(SCREEN_WIDTH - 2 - 2 * FONT_CHAR_WIDTH, 33);
  display.print(TR_O_LBL_RIGHT);
  display.setCursor(SCREEN_WIDTH - 2 - strlen(right) * FONT_CHAR_WIDTH, 43);
  display.print(right);
}

static void drawLean() {
  const motion::State &m = motion::state();
  const float lean = m.leanAbs;

  // More grip risk = less lean allowed before the alert.
  float safeLean = settings::leanAlertDeg() - weather::reading().gripRisk * LEAN_LOSS_PER_RISK_POINT;

  if (lean > safeLean) {
    // Blinking danger banner in place of the title (every 250 ms).
    if ((millis() / 250) % 2 == 0) {
      const int BANNER_MARGIN = 8;
      display.fillRoundRect(BANNER_MARGIN, 13, SCREEN_WIDTH - 2 * BANNER_MARGIN, 13, 3, COLOR_ON);
      display.setTextColor(COLOR_OFF);
      printCentered(TR_O_GRIP_DANGER, 16);
      display.setTextColor(COLOR_ON);
    }
  } else {
    pageTitle(TR_O_LEAN_TITLE);
  }

  bigNumber(constrain((int)lean, 0, 99));
  drawLeanSide(m.lean);
  drawLeanBar(m.lean, safeLean);
  drawLeanMaxima();
}

static void drawWeather() {
  const weather::Reading &air = weather::reading();
  pageTitle(TR_O_WEATHER_TITLE);

  dataRow(30, TR_O_AIR);
  display.print(air.temperature, 1);
  degreeSign();
  display.print('C');

  dataRow(41, TR_O_HUMIDITY);
  display.print((int)air.humidity);
  display.print(F(" %"));

  dataRow(52, TR_O_RISK);
  display.print((int)air.gripRisk);
  display.print(F(" %"));
  meter(88, 52, SCREEN_WIDTH - 88 - 4, air.gripRisk / 100.0f);
}

static void drawGForce() {
  const motion::State &m = motion::state();
  pageTitle(TR_O_G_TITLE);

  // Numbers on the left, two decimals in a column: lateral as a size with
  // its side, longitudinal signed (+ accelerating, - braking)...
  display.setCursor(4, 32);
  display.print(F("Lat"));
  display.setCursor(34, 32);
  display.print(fabsf(m.gLat), 2);
  if (fabsf(m.gLat) >= 0.05f) {
    display.print(' ');
    display.print(m.gLat > 0 ? TR_O_LBL_RIGHT[0] : TR_O_LBL_LEFT[0]);
  }

  display.setCursor(4, 46);
  display.print(F("Lon"));
  display.setCursor(m.gLong < 0 ? 28 : 34, 46);  // the minus sign sticks out left
  display.print(m.gLong, 2);

  // ...and a radar on the right: circle with crosshair, like car G-meters.
  // The inner ring is 0.5 G, where most road riding stays.
  const int CX = 95, CY = 45, RADIUS = 16;
  display.drawCircle(CX, CY, RADIUS, COLOR_ON);
  for (int angle = 0; angle < 360; angle += 30)  // dotted, lighter than the frame
    display.drawPixel(CX + (int)lroundf(7 * cosf(angle * DEG_TO_RAD)),
                      CY + (int)lroundf(7 * sinf(angle * DEG_TO_RAD)), COLOR_ON);
  display.drawFastHLine(CX - 18, CY, 37, COLOR_ON);
  display.drawFastVLine(CX, CY - 18, 37, COLOR_ON);

  // The dot follows the G vector, kept inside the radar. 14 pixels are
  // about 1 G: on the road it rarely leaves the circle.
  int dotX = constrain(CX + (int)(m.gLat * 14), CX - 14, CX + 14);
  int dotY = constrain(CY - (int)(m.gLong * 14), CY - 14, CY + 14);

  // A trail of where the dot was over the last second, one pixel per
  // frame: a braking-into-the-corner sweep reads as a curve, not a jump.
  constexpr int TRAIL = 10;
  static int8_t trailX[TRAIL], trailY[TRAIL];
  static uint8_t head = 0, filled = 0;
  for (int i = 0; i < filled; i++) display.drawPixel(CX + trailX[i], CY + trailY[i], COLOR_ON);
  trailX[head] = dotX - CX;
  trailY[head] = dotY - CY;
  head = (head + 1) % TRAIL;
  if (filled < TRAIL) filled++;

  display.fillCircle(dotX, dotY, 3, COLOR_ON);
}

// One page for both manoeuvres: title and big number follow the one in
// progress (nose up = wheelie, nose down = stoppie).
static void drawPitch() {
  const motion::State &m = motion::state();
  bool inStoppie = m.stoppie > m.wheelie;

  pageTitle(inStoppie ? TR_O_STOPPIE_TITLE : TR_O_WHEELIE_TITLE);

  int angle = constrain((int)(inStoppie ? m.stoppie : m.wheelie), 0, 99);
  bigNumber(angle);

  // Column with the zero in the middle: it fills upwards in a wheelie and
  // downwards in a stoppie. X is measured from the RIGHT edge.
  const int COLUMN_WIDTH  = 10;
  const int COLUMN_MARGIN = 8;
  const int columnX = SCREEN_WIDTH - COLUMN_MARGIN - COLUMN_WIDTH;

  display.drawRoundRect(columnX, 28, COLUMN_WIDTH, 34, 2, COLOR_ON);
  display.drawFastHLine(columnX - 2, 45, COLUMN_WIDTH + 4, COLOR_ON);  // zero mark

  int height = constrain(map(angle, 0, 45, 0, 15), 0, 15);
  if (inStoppie) display.fillRect(columnX + 2, 46, 6, height, COLOR_ON);
  else           display.fillRect(columnX + 2, 45 - height, 6, height, COLOR_ON);
}

// The raw sensor; lap timing has its own page. Three distinct states, and
// the distinction is the point: module missing, module without a fix, valid
// fix. Without a fix the frozen last position is NOT shown - it would be a
// silently wrong value.
static void drawGps() {
  const gps::Fix &position = gps::fix();
  pageTitle(TR_O_GPS_TITLE);

  // A module that never spoke is a wiring problem, not a satellite one, and
  // saying so changes what gets checked.
  if (!position.modulePresent) {
    dataRow(30, TR_O_STATUS);  display.print(TR_O_GPS_MISSING);
    dataRow(41, TR_O_MODULE);  display.print(TR_O_GPS_SILENT);
    dataRow(52, TR_O_CHECK);   display.print(TR_O_GPS_CHECK_WIRES);
    return;
  }

  if (!position.valid) {
    dataRow(30, TR_O_STATUS);  display.print(TR_O_NO_FIX);
    dataRow(41, "Sat");
    display.print(position.satellites);
    display.print(TR_O_SATS_LOCKED);
    dataRow(52, TR_O_WAIT);    display.print(TR_O_OPEN_SKY);
    return;
  }

  // Tighter rows than usual to fit lat/lon at full precision. Speed shows
  // live/record together: the direct comparison is the useful part.
  dataRow(29, TR_O_SPEED);
  display.print(position.speedKmh, 0);
  display.print('/');
  display.print(records::current().channels[CH_SPEED], 0);
  display.print(F(" km/h"));

  dataRow(38, "Lat");  display.print(position.lat, 5);
  dataRow(47, "Lon");  display.print(position.lon, 5);
  dataRow(56, "Sat");  display.print(position.satellites);
}

// [OK] here sets the finish line on the current GPS position.
static void drawLaps() {
  pageTitleOrTrack(TR_O_LAPS_TITLE);

  if (!laps::course().finishSet) {
    dataRow(41, TR_O_STATUS);  display.print(TR_O_LAPS_NOT_SET);
    dataRow(52, TR_O_START);   display.print(TR_O_KEY_OK);
    return;
  }

  // First row: the delta against the reference lap while there is one to
  // compare with, otherwise the distance to the real next target (a
  // checkpoint if the course has some left in this lap, or the finish line).
  // Ahead of the reference the value is shown in reverse video: good news
  // should not need reading.
  int32_t delta;
  if (tracks::delta(delta)) {
    dataRow(29, "Delta");
    const bool ahead = delta < 0;
    if (ahead) {
      display.fillRect(VALUE_X - 2, 28, SCREEN_WIDTH - VALUE_X, 9, COLOR_ON);
      display.setTextColor(COLOR_OFF);
    }
    display.print(ahead ? '-' : '+');
    display.print(fabsf(delta / 1000.0f), 2);
    display.print(F(" s"));
    display.setTextColor(COLOR_ON);
  } else {
    dataRow(29, "Dist");
    if (gps::fix().valid) {
      double lat, lon;
      laps::nextTarget(lat, lon);
      display.print(gps::distanceTo(lat, lon), 0);
      display.print(F(" m"));
    } else {
      display.print(TR_O_LAPS_NO_FIX);
    }
  }

  dataRow(38, TR_O_LAP);
  if (laps::lapRunning()) {
    uint32_t seconds = laps::currentLapMs() / 1000;
    display.printf("%lu:%02lu", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));

    // Which lap this is, right-aligned.
    char number[13];  // room for any int, which keeps -Wformat-truncation quiet
    snprintf(number, sizeof(number), "#%d", laps::lapCount() + 1);
    display.setCursor(SCREEN_WIDTH - 2 - strlen(number) * FONT_CHAR_WIDTH, 38);
    display.print(number);
  } else {
    display.print(TR_O_LAP_WAITING);
  }

  dataRow(47, TR_O_LAST);
  if (laps::lapCount() > 0) printLapTime(laps::lastLapMs());
  else display.print(F("--"));

  dataRow(56, TR_O_BEST);
  if (laps::bestLapIndex() >= 0) printLapTime(laps::bestLapMs());
  else display.print(F("--"));
}

// All-time records, on two pages: five rows would fit only at an unreadable
// 7 px pitch. [OK] deliberately does nothing here - wiping the history with
// an accidental press (maybe with gloves on) would be irreversible, so the
// reset is only on the website, with a confirmation.
static void drawRecords1() {
  const float *best = records::current().channels;
  pageTitleOrTrack(TR_O_RECORDS_1);

  doubleRow(28, TR_O_ROW_LEAN,
            TR_O_LBL_RIGHT,   best[CH_LEAN_RIGHT], false,
            TR_O_LBL_LEFT,    best[CH_LEAN_LEFT],  false, 0);
  doubleRow(39, TR_O_ROW_PITCH,
            TR_O_LBL_WHEELIE, best[CH_WHEELIE],    false,
            TR_O_LBL_STOPPIE, best[CH_STOPPIE],    false, 0);
  doubleRow(50, TR_O_ROW_GLAT,
            TR_O_LBL_RIGHT,   best[CH_GLAT_RIGHT], false,
            TR_O_LBL_LEFT,    best[CH_GLAT_LEFT],  false, 1);
}

static void drawRecords2() {
  const float *best = records::current().channels;
  pageTitleOrTrack(TR_O_RECORDS_2);

  doubleRow(34, TR_O_ROW_GLON,
            TR_O_LBL_ACCEL, best[CH_G_ACCEL], false,
            TR_O_LBL_BRAKE, best[CH_G_BRAKE], false, 1);

  dataRow(50, TR_O_SPEED);
  display.print(best[CH_SPEED], 0);
  display.print(F(" km/h"));
}

static void drawStorage() {
  pageTitle(TR_O_STORAGE_TITLE);

  if (!storage::ok()) {
    dataRow(41, "Flash");
    display.print(TR_O_ERROR);
    return;
  }

  // How full, then what is left in the two units that matter: space, and
  // hours of recording at the rate set now - the one a rider plans with.
  const uint64_t total = storage::totalBytes(), used = storage::usedBytes();
  dataRow(29, storage::onCard() ? "SD" : "Flash");
  meter(VALUE_X, 29, SCREEN_WIDTH - VALUE_X - 4, total ? (float)used / total : 1.0f);

  char size[16];
  formatSize(storage::freeBytes(), size, sizeof(size));
  dataRow(38, TR_O_FREE);
  display.print(size);

  uint32_t minutes = storage::freeBytes() / (60ULL * settings::logRateHz() * sizeof(core::SessionSample));
  dataRow(47, TR_O_TIME_LEFT);
  display.print(minutes / 60);   display.print('h');
  display.print(minutes % 60);   display.print('m');

  // The recording time is in the status bar already: here, which file.
  if (recorder::isRecording()) {
    dataRow(56, TR_O_FILE);
    display.print(recorder::fileName() + 1);  // skip the "/"
  } else {
    dataRow(56, TR_O_START);
    display.print(TR_O_KEY_LOG);
  }
}

// [OK] here turns the access point on and off.
static void drawWiFi() {
  pageTitle(TR_O_WIFI_TITLE);

  dataRow(29, TR_O_NETWORK);
  display.print(settings::wifiName());

  if (web::isActive()) {
    // Everything needed to connect (tighter rows: four values here).
    dataRow(38, TR_O_PASSWORD);  display.print(settings::wifiPassword());
    dataRow(47, TR_O_SITE);      display.print(web::address());
    dataRow(56, TR_O_CLIENTS);
    display.print(web::clientCount());
    display.print(TR_O_OK_OFF);
  } else {
    dataRow(41, TR_O_STATUS);    display.print(TR_O_OFF);
    dataRow(52, TR_O_START);     display.print(TR_O_KEY_OK);
  }
}

// ===========================================================================
// WiFi QR code
// ===========================================================================
// The standard "WIFI:" payload that phone cameras understand: pointing the
// camera at the panel joins the network, no password to type. The website
// then opens by itself (the access point answers as a captive portal, see
// src/web/Web.cpp).
//
// The encoder is the one shipped with the ESP32 core (esp_qrcode). It hands
// the finished symbol to a callback with no user pointer, hence the static
// copy below - which is also a cache: encoding runs only when name or
// password change, not at every frame.

constexpr int QR_MAX_SIZE = 33;  // version 4: the largest that fits 64 px

static char    qrPayload[64] = "";
static uint8_t qrSize = 0;       // modules per side, 0 = not encoded
static uint8_t qrBits[(QR_MAX_SIZE * QR_MAX_SIZE + 7) / 8];

static void storeQr(esp_qrcode_handle_t code) {
  int size = esp_qrcode_get_size(code);
  if (size > QR_MAX_SIZE) return;
  memset(qrBits, 0, sizeof(qrBits));
  for (int y = 0; y < size; y++)
    for (int x = 0; x < size; x++)
      if (esp_qrcode_get_module(code, x, y)) {
        int bit = y * size + x;
        qrBits[bit / 8] |= 1 << (bit % 8);
      }
  qrSize = size;
}

// In the WIFI: format the characters \ ; , " : must be escaped.
static void appendEscaped(char *out, size_t size, const char *text) {
  size_t n = strlen(out);
  for (const char *c = text; *c && n + 2 < size; c++) {
    if (strchr("\\;,\":", *c)) out[n++] = '\\';
    out[n++] = *c;
  }
  out[n] = '\0';
}

static void updateQr() {
  char payload[sizeof(qrPayload)] = "WIFI:T:WPA;S:";
  appendEscaped(payload, sizeof(payload), settings::wifiName());
  strlcat(payload, ";P:", sizeof(payload));
  appendEscaped(payload, sizeof(payload), settings::wifiPassword());
  strlcat(payload, ";;", sizeof(payload));
  if (strcmp(payload, qrPayload) == 0) return;

  strlcpy(qrPayload, payload, sizeof(qrPayload));
  qrSize = 0;
  esp_qrcode_config_t config = {};  // the header's default macro is C-only syntax
  config.display_func       = storeQr;
  config.max_qrcode_version = 4;
  config.qrcode_ecc_level   = ESP_QRCODE_ECC_LOW;
  esp_qrcode_generate(&config, qrPayload);
}

// Full screen, no status bar: the code needs every row. Dark modules on a
// lit square, as scanners expect, with a lit margin around it.
static void drawWiFiQr() {
  updateQr();
  if (qrSize == 0) {  // a name or password too long to encode: say so
    printCentered(TR_O_WIFI_TITLE, 28);
    printCentered("QR ?", 40);
    return;
  }

  const int scale = SCREEN_HEIGHT / (qrSize + 2);  // 2 px per module up to version 3
  const int side  = qrSize * scale;
  const int frame = SCREEN_HEIGHT;                 // the lit square
  const int ox    = (frame - side) / 2;

  display.fillRect(0, 0, frame, frame, COLOR_ON);
  for (int y = 0; y < qrSize; y++)
    for (int x = 0; x < qrSize; x++) {
      int bit = y * qrSize + x;
      if (qrBits[bit / 8] & (1 << (bit % 8)))
        display.fillRect(ox + x * scale, ox + y * scale, scale, scale, COLOR_OFF);
    }

  const int TEXT_X = frame + 4;
  display.setCursor(TEXT_X, 6);   display.print(TR_O_WIFI_TITLE);
  display.drawFastHLine(TEXT_X, 16, SCREEN_WIDTH - TEXT_X - 2, COLOR_ON);
  display.setCursor(TEXT_X, 22);  display.print(TR_O_QR_1);
  display.setCursor(TEXT_X, 32);  display.print(TR_O_QR_2);
  display.setCursor(TEXT_X, 42);  display.print(TR_O_QR_3);

  // The network name, cut to what fits beside the code.
  char name[(SCREEN_WIDTH - TEXT_X) / FONT_CHAR_WIDTH + 1];
  strlcpy(name, settings::wifiName(), sizeof(name));
  display.setCursor(TEXT_X, 56);
  display.print(name);
}

void composePage() {
  display.clearDisplay();

  // The QR page belongs to the WiFi: once that is off, it is gone.
  if (page == Page::WiFiQr && !web::isActive()) page = Page::WiFi;
  if (page == Page::WiFiQr) {
    drawWiFiQr();
    return;
  }

  drawStatusBar();
  switch (page) {
    case Page::Lean:     drawLean();     break;
    case Page::Weather:  drawWeather();  break;
    case Page::GForce:   drawGForce();   break;
    case Page::Pitch:    drawPitch();    break;
    case Page::Gps:      drawGps();      break;
    case Page::Laps:     drawLaps();     break;
    case Page::Records1: drawRecords1(); break;
    case Page::Records2: drawRecords2(); break;
    case Page::Storage:  drawStorage();  break;
    case Page::WiFi:     drawWiFi();     break;
    case Page::WiFiQr:
    case Page::Count:    break;
  }
  // The QR page has no title to mark: the marker counts the other ten.
  pageMarker((int)page, PAGE_COUNT - 1);
}

Page currentPage() { return page; }

void render() {
  if (!available()) return;
  composePage();
  display.display();
}

// ===========================================================================
// Page transition
// ===========================================================================
// The new page slides in pushing the old one out. With ten pages in a row
// and a screen that changes in a single frame there is no telling whether
// one is going forwards or backwards; the slide direction says it.
//
// The slide is horizontal because in the buffer one byte holds eight pixels
// in a column (x, y..y+7): shifting in x costs one memcpy per band, shifting
// in y would mean recombining the bits of two bytes for every pixel.
//
// Both frames are kept aside because composing writes into the panel
// buffer, which is also the source being copied. They cost 2 KB of RAM.

constexpr uint8_t  BUFFER_BANDS = SCREEN_HEIGHT / 8;
constexpr uint16_t BUFFER_BYTES = SCREEN_WIDTH * BUFFER_BANDS;

static uint8_t outgoingFrame[BUFFER_BYTES];
static uint8_t incomingFrame[BUFFER_BYTES];

// Each frame costs the I2C transfer of the whole screen (~23 ms): 16 px are
// seven frames, ~200 ms overall. Finer is smoother but feels like lag
// between the press and the page.
constexpr int      TRANSITION_STEP_PX = 16;
constexpr uint32_t TRANSITION_PAUSE_MS = 5;

static void slideTo(int direction, bool barStays) {
  // The page being left is already on screen.
  memcpy(outgoingFrame, display.getBuffer(), BUFFER_BYTES);

  // The incoming one is drawn once: values stay still during the slide, a
  // number changing while the page moves reads worse.
  composePage();
  memcpy(incomingFrame, display.getBuffer(), BUFFER_BYTES);

  uint8_t *frame = display.getBuffer();
  for (int shifted = TRANSITION_STEP_PX; shifted < SCREEN_WIDTH; shifted += TRANSITION_STEP_PX) {
    int remaining = SCREEN_WIDTH - shifted;  // columns of the old page still visible

    for (uint8_t band = 0; band < BUFFER_BANDS; band++) {
      uint16_t start = band * SCREEN_WIDTH;
      uint8_t *row = frame + start;

      if (direction > 0) {
        memcpy(row,             outgoingFrame + start + shifted, remaining);
        memcpy(row + remaining, incomingFrame + start,           shifted);
      } else {
        // The TAIL of the new page comes in first.
        memcpy(row,           incomingFrame + start + remaining, shifted);
        memcpy(row + shifted, outgoingFrame + start,             remaining);
      }
    }

    // Still, on top of the composed frame - unless one of the two pages
    // has no bar (the QR code), and then it slides along with its page.
    if (barStays) drawStatusBar();
    display.display();
    delay(TRANSITION_PAUSE_MS);
  }

  // Not a repetition: if the step does not divide the width, the loop ends
  // with a few columns of the old page still on screen.
  memcpy(frame, incomingFrame, BUFFER_BYTES);
  display.display();
}

void changePage(int direction) {
  const Page from = page;
  // "+ PAGE_COUNT" keeps the modulo from going negative.
  auto step = [&]() {
    page = (Page)(((int)page + PAGE_COUNT + (direction > 0 ? 1 : -1)) % PAGE_COUNT);
  };
  step();
  if (page == Page::WiFiQr && !web::isActive()) step();  // only there with the WiFi on

  if (available()) slideTo(direction, from != Page::WiFiQr && page != Page::WiFiQr);
}

// ===========================================================================
// Popup
// ===========================================================================
// It opens growing from the centre, so it reads as "something just
// happened" rather than a screen glitch. Each frame is larger than the
// previous one and centred, so its black fill covers the previous border by
// itself: the page underneath needs no redraw.
void popup(const char *text) {
  if (!available()) return;

  const int POPUP_MARGIN = 9;
  const int WIDTH        = SCREEN_WIDTH - 2 * POPUP_MARGIN;
  const int HEIGHT       = 28;
  const int CENTER_Y     = 32;
  const int OPEN_STEPS   = 4;

  for (int step = 1; step <= OPEN_STEPS; step++) {
    int w = WIDTH  * step / OPEN_STEPS;
    int h = HEIGHT * step / OPEN_STEPS;
    int x = SCREEN_WIDTH / 2 - w / 2;
    int y = CENTER_Y - h / 2;

    display.fillRoundRect(x, y, w, h, 4, COLOR_OFF);
    display.drawRoundRect(x, y, w, h, 4, COLOR_ON);
    display.display();
  }

  printCentered(text, CENTER_Y - 4);
  display.display();
  delay(POPUP_MS);
}

// A message that stays until the next render(): for the moments in which
// the firmware is about to be busy and will not redraw for a while.
void notice(const char *text) {
  if (!available()) return;

  display.clearDisplay();
  drawStatusBar();
  display.drawRoundRect(4, 22, SCREEN_WIDTH - 8, 28, 4, COLOR_ON);
  printCentered(text, 32);
  display.display();
}

}  // namespace oled
