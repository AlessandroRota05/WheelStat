#pragma once

/*
  WheelStat - src/core/Geo.h
  ===========================================================================
  Flat-earth geometry around a point: over the area of a circuit the Earth
  is flat enough, and metres are far easier to reason about than degrees.

  Everything in src/core is plain C++ with no Arduino dependency. It is
  compiled into the firmware and, unchanged, into the tests that run on a
  PC (see test/).
  ===========================================================================
*/

#include <math.h>
#include <stdint.h>

namespace core {

// Metres east (x) and north (y) of an origin.
struct Point {
  float x;
  float y;
};

constexpr double METRES_PER_DEGREE  = 111320.0;
constexpr double RADIANS_PER_DEGREE = 0.017453292519943295;

inline Point toLocal(double originLat, double originLon, double lat, double lon) {
  Point p;
  p.x = (float)((lon - originLon) * METRES_PER_DEGREE * cos(originLat * RADIANS_PER_DEGREE));
  p.y = (float)((lat - originLat) * METRES_PER_DEGREE);
  return p;
}

inline float dot(Point a, Point b)   { return a.x * b.x + a.y * b.y; }
inline float cross(Point a, Point b) { return a.x * b.y - a.y * b.x; }

// Unit vector of a compass heading (0 = north, 90 = east).
inline Point headingVector(float compassDeg) {
  float angle = compassDeg * (float)RADIANS_PER_DEGREE;
  return { sinf(angle), cosf(angle) };
}

// Stored positions are degrees * 1e7 in an int32 (about 1 cm). A float in
// the same four bytes only resolves half a metre at these latitudes.
inline int32_t toE7(double degrees)  { return (int32_t)lround(degrees * 1e7); }
inline double  fromE7(int32_t value) { return value / 1e7; }

}  // namespace core
