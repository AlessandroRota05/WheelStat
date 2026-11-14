// WheelStat - OLED boot screens: logo, component checklist, IMU calibration.

#include "Oled.h"

#include "../Buttons.h"
#include "../Version.h"
#include "../i18n/Text.h"
#include "OledDraw.h"

namespace oled {

static bool panelOk = false;

bool begin() {
  panelOk = displayBegin();
  return panelOk;
}

bool available() { return panelOk; }

// ===========================================================================
// Logo
// ===========================================================================

// A frame already costs ~23 ms of I2C transfer at 400 kHz; FRAME_PAUSE_MS
// keeps the animation from depending on that (on an SPI panel it would race).
constexpr uint32_t FRAME_PAUSE_MS  = 5;
constexpr uint32_t LETTER_PAUSE_MS = 40;   // the name appearing, per letter
constexpr uint32_t LOGO_HOLD_MS    = 400;  // time to look at the whole logo

// Rounded frame, big name in the middle, version below.
constexpr char LOGO_NAME[]    = "WHEELSTAT";
constexpr int  LOGO_MARGIN    = 8;
constexpr int  LOGO_FRAME_Y   = 12;
constexpr int  LOGO_FRAME_H   = 40;
constexpr int  LOGO_FRAME_R   = 6;
constexpr int  LOGO_NAME_Y    = 24;
constexpr int  LOGO_VERSION_Y = 42;

// One frame of the logo. The animation is the sequence of calls below:
// skipping it just means drawing the last frame.
static void drawLogo(int frameWidth, int letters, bool withVersion) {
  display.clearDisplay();
  display.drawRoundRect((SCREEN_WIDTH - frameWidth) / 2, LOGO_FRAME_Y,
                        frameWidth, LOGO_FRAME_H, LOGO_FRAME_R, COLOR_ON);

  display.setTextColor(COLOR_ON);
  display.setTextSize(2);
  display.setCursor(centerX(LOGO_NAME, 2), LOGO_NAME_Y);
  for (int i = 0; i < letters; i++) display.print(LOGO_NAME[i]);
  display.setTextSize(1);

  if (withVersion) printCentered(FIRMWARE_VERSION, LOGO_VERSION_Y);
  display.display();
}

void splashIntro() {
  if (!panelOk) return;

  const int FULL_WIDTH = SCREEN_WIDTH - 2 * LOGO_MARGIN;
  const int LETTERS    = strlen(LOGO_NAME);
  bool skipped = false;

  // 1. the frame opens from the centre. The step divides the width exactly,
  //    so the last frame is the full one.
  for (int width = 16; width <= FULL_WIDTH && !skipped; width += 12) {
    drawLogo(width, 0, false);
    skipped = buttons::anyPressed();
    delay(FRAME_PAUSE_MS);
  }

  // 2. the name is typed out
  for (int n = 1; n <= LETTERS && !skipped; n++) {
    drawLogo(FULL_WIDTH, n, false);
    skipped = buttons::anyPressed();
    delay(LETTER_PAUSE_MS);
  }

  // 3. the version, then a moment to look at the whole logo
  drawLogo(FULL_WIDTH, LETTERS, true);
  if (!skipped) delay(LOGO_HOLD_MS);

  buttons::holdOff();
}

// ===========================================================================
// Component checklist
// ===========================================================================

constexpr int CHECK_FIRST_ROW_Y = 15;
constexpr int CHECK_ROW_STEP    = 10;
constexpr int CHECK_LABEL_X     = 2;
constexpr int CHECK_NAME_X      = 36;
constexpr int CHECK_RESULT_X    = 106;

// Initialisations are almost instant: without this the row would go from
// empty to ticked with nothing to see.
constexpr uint32_t CHECK_PENDING_MS = 120;

static void clearResultCell(int y) {
  display.fillRect(CHECK_RESULT_X, y, SCREEN_WIDTH - CHECK_RESULT_X, 8, COLOR_OFF);
}

void checklistBegin() {
  if (!panelOk) return;
  display.clearDisplay();
  titleBar(TR_O_BOOT_TITLE);
  display.display();
}

void checklistPending(int row, const char *label, const char *driverName) {
  if (!panelOk) return;
  int y = CHECK_FIRST_ROW_Y + row * CHECK_ROW_STEP;

  display.setTextSize(1);
  display.setTextColor(COLOR_ON);
  display.setCursor(CHECK_LABEL_X, y);
  display.print(label);
  display.setCursor(CHECK_NAME_X, y);
  display.print(driverName);

  for (int dots = 1; dots <= 3; dots++) {
    clearResultCell(y);
    display.setCursor(CHECK_RESULT_X, y);
    for (int i = 0; i < dots; i++) display.print('.');
    display.display();
    delay(CHECK_PENDING_MS / 3);
  }
}

// A tick or a cross drawn with segments instead of "OK"/"--": readable out
// of the corner of the eye and narrower.
void checklistResult(int row, bool ok) {
  if (!panelOk) return;
  int y = CHECK_FIRST_ROW_Y + row * CHECK_ROW_STEP;
  int x = CHECK_RESULT_X + 6;

  clearResultCell(y);
  if (ok) {
    display.drawLine(x,     y + 4, x + 2, y + 6, COLOR_ON);
    display.drawLine(x + 2, y + 6, x + 7, y + 1, COLOR_ON);
  } else {
    display.drawLine(x,     y + 1, x + 6, y + 7, COLOR_ON);
    display.drawLine(x + 6, y + 1, x,     y + 7, COLOR_ON);
  }
  display.display();
}

// ===========================================================================
// IMU calibration
// ===========================================================================

// Two circles touching at (EIGHT_CX, 35), the crossing of the eight. All of
// it left of x=52: the right column belongs to the MAG value.
constexpr int EIGHT_CX     = 24;
constexpr int EIGHT_TOP_CY = 26;
constexpr int EIGHT_BOT_CY = 44;
constexpr int EIGHT_RADIUS = 9;
constexpr int ARROW_X      = 40;  // just outside the dot's path
constexpr int DOT_RADIUS   = 3;

// The 16 dot positions in travel order: top circle clockwise, crossing,
// bottom circle anticlockwise, and back. Written out by hand - they never
// change, and no trigonometry is needed. With fewer than 16 the dot jumps
// instead of gliding.
constexpr int8_t EIGHT_PATH[][2] = {
  {24, 17}, {30, 20}, {33, 26}, {30, 32},
  {24, 35},
  {18, 38}, {15, 44}, {18, 50},
  {24, 53}, {30, 50}, {33, 44}, {30, 38},
  {24, 35},
  {18, 32}, {15, 26}, {18, 20}
};
constexpr uint8_t  EIGHT_STEPS   = sizeof(EIGHT_PATH) / sizeof(EIGHT_PATH[0]);
constexpr uint32_t EIGHT_STEP_MS = 90;  // a full loop in ~1.4 s

// Shaft plus solid head. direction = +1 points down, -1 up; the shaft is
// always on the side opposite the head, so the arrow arrives at the point
// it indicates instead of crossing it.
static void verticalArrow(int x, int baseY, int direction) {
  display.drawFastVLine(x, direction > 0 ? baseY - 8 : baseY, 9, COLOR_ON);
  display.fillTriangle(x - 3, baseY, x + 3, baseY, x, baseY + 6 * direction, COLOR_ON);
}

void drawCalibration(uint8_t level) {
  if (!panelOk) return;

  display.clearDisplay();
  titleBar(TR_O_CAL_TITLE);

  display.drawCircle(EIGHT_CX, EIGHT_TOP_CY, EIGHT_RADIUS, COLOR_ON);
  display.drawCircle(EIGHT_CX, EIGHT_BOT_CY, EIGHT_RADIUS, COLOR_ON);

  // The arrows sit on the right flank, where the path goes down the top
  // circle and up the bottom one: they point at each other and meet at the
  // crossing. At the ends of the eight the direction would be ambiguous.
  verticalArrow(ARROW_X, EIGHT_TOP_CY, +1);
  verticalArrow(ARROW_X, EIGHT_BOT_CY, -1);

  const uint8_t step = (millis() / EIGHT_STEP_MS) % EIGHT_STEPS;
  display.fillCircle(EIGHT_PATH[step][0], EIGHT_PATH[step][1], DOT_RADIUS, COLOR_ON);

  // Big enough to read while waving the box in the air.
  display.setTextSize(2);
  display.setCursor(58, 14);
  display.print(F("MAG"));

  display.setTextSize(3);
  display.setCursor(58, 32);
  display.print(level);
  display.print(F("/3"));
  display.setTextSize(1);

  printCentered(TR_O_CAL_SKIP, 56);
  display.display();
}

void drawCalibrationDone() {
  if (!panelOk) return;

  display.clearDisplay();
  titleBar(TR_O_CAL_TITLE);
  printCentered(TR_O_CAL_DONE, 22, 2);
  printCentered("MAG 3/3", 42);
  printCentered(TR_O_CAL_ANY_KEY, 56);
  display.display();
}

}  // namespace oled
