// WheelStat - OLED end-of-session summary and the blocking key wait.

#include "Oled.h"

#include "../Buttons.h"
#include "../Channels.h"
#include "../Recorder.h"
#include "../Records.h"
#include "../Stats.h"
#include "../Tuning.h"
#include "../i18n/Text.h"
#include "../web/Web.h"
#include "OledDraw.h"

namespace oled {

// The first moments are ignored, otherwise the bounce of the key that was
// just released would close the screen at once.
constexpr uint32_t KEY_GUARD_MS = 500;

bool waitAnyKey() {
  uint32_t entered = millis();
  while (millis() - entered < SUMMARY_TIMEOUT_MS) {
    // The phone must be able to browse during the wait too.
    web::handleClient();

    if (buttons::anyPressed() && millis() - entered > KEY_GUARD_MS) return true;
    delay(LOOP_PAUSE_MS);
  }
  return false;
}

// Angles with no decimals and G with one: more digits cannot be read at a
// glance on the OLED, full precision stays in the log. The asterisk marks a
// freshly beaten all-time record.
static void drawMaxima1() {
  const float *best = stats::sessionMax();

  display.clearDisplay();
  titleBar(TR_O_SUM_MAX_1);

  doubleRow(14, TR_O_ROW_LEAN,
            TR_O_LBL_RIGHT,   best[CH_LEAN_RIGHT], records::isNew(CH_LEAN_RIGHT),
            TR_O_LBL_LEFT,    best[CH_LEAN_LEFT],  records::isNew(CH_LEAN_LEFT), 0);
  doubleRow(26, TR_O_ROW_PITCH,
            TR_O_LBL_WHEELIE, best[CH_WHEELIE],    records::isNew(CH_WHEELIE),
            TR_O_LBL_STOPPIE, best[CH_STOPPIE],    records::isNew(CH_STOPPIE), 0);
  doubleRow(38, TR_O_ROW_GLAT,
            TR_O_LBL_RIGHT,   best[CH_GLAT_RIGHT], records::isNew(CH_GLAT_RIGHT),
            TR_O_LBL_LEFT,    best[CH_GLAT_LEFT],  records::isNew(CH_GLAT_LEFT), 1);

  display.setCursor(0, 56);
  display.print(TR_O_SUM_CONTINUE);
  display.display();
}

// The "new record" legend looks at every channel, not only those shown here.
static void drawMaxima2() {
  const float *best = stats::sessionMax();
  uint32_t seconds = recorder::durationSeconds();

  display.clearDisplay();
  titleBar(TR_O_SUM_MAX_2);

  doubleRow(16, TR_O_ROW_GLON,
            TR_O_LBL_ACCEL, best[CH_G_ACCEL], records::isNew(CH_G_ACCEL),
            TR_O_LBL_BRAKE, best[CH_G_BRAKE], records::isNew(CH_G_BRAKE), 1);

  dataRow(30, TR_O_SPEED);
  display.print(best[CH_SPEED], 0);
  display.print(F(" km/h"));
  if (records::isNew(CH_SPEED)) display.print('*');

  display.setCursor(0, 56);
  display.print(TR_O_SUM_DURATION);
  display.printf("%lu:%02lu", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));

  display.setCursor(80, 56);
  display.print(records::anyNew() ? TR_O_SUM_NEW : TR_O_SUM_KEY);
  display.display();
}

static void drawEventCounts() {
  display.clearDisplay();
  titleBar(TR_O_SUM_EVENTS);

  for (int e = 0; e < EVENT_COUNT; e++) {
    display.setCursor(0, 15 + e * 10);
    display.print(eventInfo(e).oledLabel);
    display.print(stats::eventCount(e));
  }
  display.display();
}

void showSessionSummary() {
  if (available()) {
    drawMaxima1();
    if (waitAnyKey()) {
      drawMaxima2();
      if (waitAnyKey()) {
        drawEventCounts();
        waitAnyKey();
      }
    }
  }
  buttons::holdOff();
}

}  // namespace oled
