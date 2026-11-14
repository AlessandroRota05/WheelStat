#pragma once

/*
  WheelStat - src/drivers/Display.h
  ===========================================================================
  OLED panel: the concrete type and its power-up sequence, the only things
  that are specific to the controller. All drawing is done elsewhere
  (src/ui) in plain Adafruit_GFX primitives, common to every monochrome
  panel - which is why this driver is so short.

  DisplayDriver must expose getBuffer(), the pointer to the frame in RAM:
  the page transition scrolls two frames into each other with memcpy. Both
  Adafruit_SSD1306 and Adafruit_GrayOLED have it.

  Only the SSD1306 is tested on hardware. The other three compile; check
  them on first assembly, the address above all (0x3C or 0x3D).
  ===========================================================================
*/

#include "../../Config.h"

#if defined(DISPLAY_SSD1306) || defined(DISPLAY_SSD1309)
  #include <Adafruit_SSD1306.h>
  typedef Adafruit_SSD1306 DisplayDriver;
  constexpr uint16_t COLOR_ON  = SSD1306_WHITE;
  constexpr uint16_t COLOR_OFF = SSD1306_BLACK;
#elif defined(DISPLAY_SH1106) || defined(DISPLAY_SH1107)
  // One library for two controllers, but two classes: the panel's internal
  // memory addressing differs.
  #include <Adafruit_SH110X.h>
  #if defined(DISPLAY_SH1106)
    typedef Adafruit_SH1106G DisplayDriver;
  #else
    typedef Adafruit_SH1107 DisplayDriver;
  #endif
  constexpr uint16_t COLOR_ON  = SH110X_WHITE;
  constexpr uint16_t COLOR_OFF = SH110X_BLACK;
#endif

extern DisplayDriver display;

// Powers the panel up. False if nothing answers on the I2C address.
// Wire.begin() must already have been called.
bool displayBegin();

// Name shown on the boot checklist (about 11 characters at most).
const char *displayName();
