#include "Gate.h"

#include "../Tuning.h"

namespace core {

// True if the segment a -> b crosses, in the sense of `direction`, the line
// through the origin at right angles to it, within the gate width.
static bool crossesLine(Point a, Point b, Point direction, float &fraction) {
  float before = dot(a, direction);
  float after  = dot(b, direction);
  if (!(before < 0.0f && after >= 0.0f)) return false;

  fraction = -before / (after - before);
  Point at = { a.x + (b.x - a.x) * fraction, a.y + (b.y - a.y) * fraction };
  return fabsf(cross(direction, at)) <= GATE_HALF_WIDTH_M;
}

Crossing detectCrossing(const Gate &gate, const GpsSample &prev, const GpsSample &curr) {
  Crossing result = { Pass::None, 0.0f };

  Point a = toLocal(gate.lat, gate.lon, prev.lat, prev.lon);
  Point b = toLocal(gate.lat, gate.lon, curr.lat, curr.lon);

  if (gate.confidence == GATE_UNKNOWN) {
    if (curr.speedKmh < GATE_LEARN_SPEED_KMH) return result;

    // The line is taken across the rider's own course. "Just past it" also
    // counts: the gate is usually marked standing on it, and the learning
    // speed is reached a few metres later.
    Point travel = headingVector(curr.courseDeg);
    float before = dot(a, travel);
    float after  = dot(b, travel);
    if (after < 0.0f || before > GATE_LEARN_RUNOUT_M) return result;
    if (fabsf(cross(travel, b)) > GATE_HALF_WIDTH_M) return result;

    result.pass     = Pass::Learned;
    result.fraction = (before < 0.0f) ? -before / (after - before) : 0.0f;
    return result;
  }

  Point direction = headingVector(gate.headingDeg);
  if (curr.speedKmh >= GATE_MIN_SPEED_KMH && crossesLine(a, b, direction, result.fraction)) {
    result.pass = Pass::Forward;
    return result;
  }

  if (gate.confidence == GATE_PROVISIONAL && curr.speedKmh >= GATE_LEARN_SPEED_KMH) {
    Point opposite = { -direction.x, -direction.y };
    if (crossesLine(a, b, opposite, result.fraction)) result.pass = Pass::Reversed;
  }
  return result;
}

}  // namespace core
