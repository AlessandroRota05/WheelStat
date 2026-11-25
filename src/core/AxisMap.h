#pragma once

// WheelStat - src/core/AxisMap.h
// Which axis of the chip points forward, left and up on the bike, with its
// sign: how a chip-frame vector becomes a bike-frame one. Codes are the
// axis number (1 = X, 2 = Y, 3 = Z), negative when the chip axis points
// the other way. {1, 2, 3} is a breakout flat, components up, X forward.

#include <stdint.h>

namespace core {

struct AxisMap {
  int8_t forward, left, up;

  void apply(const float chip[3], float bike[3]) const {
    bike[0] = pick(chip, forward);
    bike[1] = pick(chip, left);
    bike[2] = pick(chip, up);
  }

  // A valid map uses each chip axis once.
  bool valid() const {
    int a = forward < 0 ? -forward : forward;
    int b = left < 0 ? -left : left;
    int c = up < 0 ? -up : up;
    return a >= 1 && a <= 3 && b >= 1 && b <= 3 && c >= 1 && c <= 3 && a != b && b != c && a != c;
  }

 private:
  static float pick(const float v[3], int8_t code) {
    return code > 0 ? v[code - 1] : -v[-code - 1];
  }
};

}  // namespace core
