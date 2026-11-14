#pragma once

// WheelStat - drawing helpers shared by the OLED source files. Internal to
// src/ui: the rest of the firmware only includes Oled.h.
//
// The visual language is the same on every page: title centred at y=15,
// separator at y=24, data below; labels at x=6 and values in a column at
// x=54, so every page reads the same way.

#include <Arduino.h>

#include "../drivers/Display.h"

namespace oled {

constexpr int LABEL_X = 6;   // where a row's label starts
constexpr int VALUE_X = 54;  // where its value starts

// The x a string must start from to end up centred. Computed here, not by
// the caller: changing a text or SCREEN_WIDTH needs no sums redone by hand.
int centerX(const char *text, uint8_t textSize = 1);
void printCentered(const char *text, int y, uint8_t textSize = 1);

// Inverted title bar (white background, black text) of the full-screen
// screens: boot, calibration, summary.
void titleBar(const char *text);

// Page title with its separator line.
void pageTitle(const char *text);

// The active track's name as the title, or `fallback` in free mode.
void pageTitleOrTrack(const char *fallback);

// Starts a "label / value" row: prints the label and leaves the cursor on
// the value column for the caller.
void dataRow(int y, const char *label);

// Quantity name on the left and two labelled values on FIXED columns (x=42
// and x=86), the same on every row and page that uses it. newA/newB add the
// "new record" asterisk. The worst case (7 characters at x=86) ends exactly
// on the right edge.
void doubleRow(int y, const char *name,
               const char *labelA, float valueA, bool newA,
               const char *labelB, float valueB, bool newB,
               int decimals);

// Large centred number with a degree symbol.
void bigNumber(int value);

// A degree sign at the text cursor, for the size-1 font.
void degreeSign();

// Horizontal gauge, 7 px tall, filled by `fraction` (0..1).
void meter(int x, int y, int width, float fraction);

// Thick stretch on the title separator saying which of `count` pages this
// is: with ten pages in a loop, where one is matters more than its number.
void pageMarker(int index, int count);

void printLapTime(uint32_t ms);

// Status bar on top of every live page. Drawn by the transition too, over
// each frame of the slide: it is shared frame and must not slide away.
void drawStatusBar();

// Composes the current page INTO THE BUFFER without sending it.
void composePage();

}  // namespace oled
