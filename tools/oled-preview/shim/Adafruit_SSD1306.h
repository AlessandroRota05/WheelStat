#pragma once

// A panel that only exists in RAM: the real Adafruit_GFX draws into it, and
// display() hands each finished frame to the preview program.

#include <Adafruit_GFX.h>

#define SSD1306_BLACK 0
#define SSD1306_WHITE 1

class Adafruit_SSD1306 : public Adafruit_GFX {
 public:
  Adafruit_SSD1306() : Adafruit_GFX(128, 64) {}
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= 128 || y >= 64) return;
    uint8_t &cell = buffer_[x + (y / 8) * 128];
    if (color) cell |= (1 << (y & 7));
    else       cell &= ~(1 << (y & 7));
  }
  void clearDisplay() { memset(buffer_, 0, sizeof(buffer_)); }
  void display();  // in preview.cpp
  uint8_t *getBuffer() { return buffer_; }
 private:
  uint8_t buffer_[128 * 64 / 8] = {};
};
