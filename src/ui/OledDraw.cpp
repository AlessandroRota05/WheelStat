#include "OledDraw.h"

#include "../Tracks.h"
#include "../Util.h"

namespace oled {

constexpr int TITLE_MARGIN = 10;  // separator line inset

int centerX(const char *text, uint8_t textSize) {
  int width = strlen(text) * FONT_CHAR_WIDTH * textSize;
  return (SCREEN_WIDTH - width) / 2;
}

void printCentered(const char *text, int y, uint8_t textSize) {
  display.setTextSize(textSize);
  display.setCursor(centerX(text, textSize), y);
  display.print(text);
  display.setTextSize(1);
}

void titleBar(const char *text) {
  display.fillRect(0, 0, SCREEN_WIDTH, 11, COLOR_ON);
  display.setTextColor(COLOR_OFF);
  printCentered(text, 2);
  display.setTextColor(COLOR_ON);
}

void pageTitle(const char *text) {
  printCentered(text, 15);
  display.drawFastHLine(TITLE_MARGIN, 24, SCREEN_WIDTH - 2 * TITLE_MARGIN, COLOR_ON);
}

void pageTitleOrTrack(const char *fallback) {
  if (!tracks::hasActive()) {
    pageTitle(fallback);
    return;
  }
  char title[15];  // the name, cut to 14 characters
  strlcpy(title, tracks::active().name, sizeof(title));
  pageTitle(title);
}

void dataRow(int y, const char *label) {
  display.setCursor(LABEL_X, y);
  display.print(label);
  display.setCursor(VALUE_X, y);
}

void doubleRow(int y, const char *name,
               const char *labelA, float valueA, bool newA,
               const char *labelB, float valueB, bool newB,
               int decimals) {
  display.setCursor(0, y);
  display.print(name);

  display.setCursor(42, y);
  display.print(labelA);
  display.print(valueA, decimals);
  if (newA) display.print('*');

  display.setCursor(86, y);
  display.print(labelB);
  display.print(valueB, decimals);
  if (newB) display.print('*');
}

void bigNumber(int value) {
  display.setTextSize(3);
  // One digit sits further in than two.
  display.setCursor(value < 10 ? 45 : 35, 30);
  display.print(value);
  display.setTextSize(1);
  display.drawCircle(77, 32, 2, COLOR_ON);  // degree sign sized to the digits
}

// The font has no degree sign (its 0xF7 is not one): a small ring, then the
// cursor moves on as if a character had been printed.
void degreeSign() {
  int16_t x = display.getCursorX(), y = display.getCursorY();
  display.drawCircle(x + 2, y + 1, 1, COLOR_ON);
  display.setCursor(x + 5, y);
}

void meter(int x, int y, int width, float fraction) {
  fraction = constrain(fraction, 0.0f, 1.0f);
  display.drawRect(x, y, width, 7, COLOR_ON);
  display.fillRect(x + 2, y + 2, (int)((width - 4) * fraction + 0.5f), 3, COLOR_ON);
}

void pageMarker(int index, int count) {
  const int span  = SCREEN_WIDTH - 2 * TITLE_MARGIN;
  const int slot  = span / count;
  display.fillRect(TITLE_MARGIN + index * slot + 1, 23, slot - 2, 3, COLOR_ON);
}

void printLapTime(uint32_t ms) {
  char text[16];
  formatLapTime(ms, text, sizeof(text));
  display.print(text);
}

}  // namespace oled
