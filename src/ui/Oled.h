#pragma once

/*
  WheelStat - src/ui/Oled.h
  ===========================================================================
  Everything that draws on the OLED: boot splash and component checklist,
  IMU calibration screen, the live pages with the transition between them,
  popups and the end-of-session summary.

  Every function returns straight away when the panel is missing: the
  device keeps working, with the serial log as its only output.

    OledBoot.cpp     splash, checklist, calibration
    OledPages.cpp    status bar, live pages, transition, popup
    OledSummary.cpp  end-of-session summary, blocking key wait
    OledDraw.*       drawing helpers shared by the three
  ===========================================================================
*/

#include <Arduino.h>

namespace oled {

// Ordered by theme: first what matters while riding (sensors, GPS, laps),
// then the history (records), then system diagnostics (storage, WiFi).
enum class Page : uint8_t {
  Lean,
  Weather,
  GForce,
  Pitch,      // wheelie / stoppie
  Gps,
  Laps,
  Records1,
  Records2,
  Storage,
  WiFi,
  WiFiQr,     // only while the WiFi is on: skipped otherwise
  Count
};

// Powers the panel up. False if it does not answer.
bool begin();
bool available();

// --- Boot -------------------------------------------------------------------
// Animated logo; any key skips to the last frame.
void splashIntro();

// Component checklist: one row per component, filled in WHILE it is really
// initialised. checklistPending() goes BEFORE the init - if the boot hangs,
// the last row with dots says on which component.
void checklistBegin();
void checklistPending(int row, const char *label, const char *driverName);
void checklistResult(int row, bool ok, const char *driverName);

// Calibration screen: shows the figure-eight to trace with the box in hand.
void drawCalibration(uint8_t level);
void drawCalibrationDone();

// --- Live pages -------------------------------------------------------------
Page currentPage();

// Moves to the next (+1) or previous (-1) page with a sliding transition
// whose direction matches the key.
void changePage(int direction);

// Redraws the current page. Call it at DISPLAY_INTERVAL_MS.
void render();

// Centred popup over the current page; blocks for POPUP_MS.
void popup(const char *text);

// A message that replaces the page and stays until the next render(). For
// when the firmware is about to be busy for a while (a firmware upload).
void notice(const char *text);

// --- Blocking screens -------------------------------------------------------
// Waits for any key, serving web clients meanwhile. True if a key was
// pressed, false on timeout.
bool waitAnyKey();

// Three summary pages; a key moves on, the timeout closes everything.
// Blocking is acceptable: a recording is always stopped while stationary.
void showSessionSummary();

}  // namespace oled
