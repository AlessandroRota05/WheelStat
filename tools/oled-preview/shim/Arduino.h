#pragma once

// The few pieces of the Arduino API the OLED code and Adafruit_GFX use,
// enough to compile them on a PC. Time is fake: delay() moves millis()
// forward, so animations and blinking are reproducible.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "Print.h"

using std::max;
using std::min;

typedef bool    boolean;
typedef uint8_t byte;

#define PROGMEM
#define PSTR(s) (s)
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
#define FPSTR(p) (reinterpret_cast<const __FlashStringHelper *>(p))
#define pgm_read_byte(a)    (*(const uint8_t *)(a))
#define pgm_read_word(a)    (*(const uint16_t *)(a))
#define pgm_read_dword(a)   (*(const uint32_t *)(a))
#define pgm_read_pointer(a) (*(void *const *)(a))

#define DEG_TO_RAD 0.017453292519943295769236907684886
#define radians(deg) ((deg) * DEG_TO_RAD)
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

inline long map(long x, long inMin, long inMax, long outMin, long outMax) {
  return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

uint32_t millis();
void     delay(uint32_t ms);

#ifndef _WIN32_STRLCPY
inline size_t strlcpy(char *dst, const char *src, size_t size) {
  size_t len = strlen(src);
  if (size > 0) {
    size_t n = len < size - 1 ? len : size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
  }
  return len;
}
inline size_t strlcat(char *dst, const char *src, size_t size) {
  size_t used = strnlen(dst, size);
  return used + strlcpy(dst + used, src, size - used);
}
#endif
