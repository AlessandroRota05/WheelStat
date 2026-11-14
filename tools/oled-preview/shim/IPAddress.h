#pragma once

#include "Print.h"

class IPAddress : public Printable {
 public:
  IPAddress(uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0) : b_{a, b, c, d} {}
  size_t printTo(Print &p) const override {
    return p.printf("%u.%u.%u.%u", b_[0], b_[1], b_[2], b_[3]);
  }
 private:
  uint8_t b_[4];
};
