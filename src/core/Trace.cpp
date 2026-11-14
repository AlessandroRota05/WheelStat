#include "Trace.h"

#include "Geo.h"

namespace core {

void TraceRecorder::clear() {
  count_  = 0;
  stride_ = 1;
  skip_   = 0;
}

void TraceRecorder::add(double lat, double lon, uint32_t timeMs) {
  if (skip_ > 0) {
    skip_--;
    return;
  }

  if (count_ == MAX_TRACE_POINTS) {
    for (int i = 0; i < MAX_TRACE_POINTS / 2; i++) points_[i] = points_[2 * i];
    count_ = MAX_TRACE_POINTS / 2;
    stride_ *= 2;
  }

  points_[count_++] = { toE7(lat), toE7(lon), timeMs };
  skip_ = stride_ - 1;
}

// Segments searched behind and ahead of the last match.
constexpr int SEARCH_BEHIND = 2;
constexpr int SEARCH_AHEAD  = 8;

// Squared distance from `p` to the segment a -> b, and how far along it
// (0..1) the nearest point is.
static float distanceToSegment(Point p, Point a, Point b, float &along) {
  Point  ab = { b.x - a.x, b.y - a.y };
  Point  ap = { p.x - a.x, p.y - a.y };
  float  lengthSquared = dot(ab, ab);

  along = (lengthSquared > 0.0f) ? dot(ap, ab) / lengthSquared : 0.0f;
  if (along < 0.0f) along = 0.0f;
  if (along > 1.0f) along = 1.0f;

  Point nearest = { a.x + ab.x * along, a.y + ab.y * along };
  Point offset  = { p.x - nearest.x, p.y - nearest.y };
  return dot(offset, offset);
}

bool DeltaTracker::compute(const TracePoint *reference, int count, double lat, double lon,
                           uint32_t lapMs, int32_t &deltaMs) {
  if (count < 2) return false;

  const double originLat = fromE7(reference[0].latE7);
  const double originLon = fromE7(reference[0].lonE7);
  auto local = [&](const TracePoint &point) {
    return toLocal(originLat, originLon, fromE7(point.latE7), fromE7(point.lonE7));
  };
  const Point position = toLocal(originLat, originLon, lat, lon);

  int   bestSegment = -1;
  float bestAlong   = 0.0f;
  float bestSquared = 0.0f;
  auto search = [&](int from, int to) {
    for (int i = from; i <= to; i++) {
      float along;
      float squared = distanceToSegment(position, local(reference[i]), local(reference[i + 1]), along);
      if (bestSegment < 0 || squared < bestSquared) {
        bestSegment = i;
        bestAlong   = along;
        bestSquared = squared;
      }
    }
  };

  const int lastSegment = count - 2;
  if (cursor_ > lastSegment) cursor_ = lastSegment;

  int from = cursor_ - SEARCH_BEHIND;
  int to   = cursor_ + SEARCH_AHEAD;
  search(from < 0 ? 0 : from, to > lastSegment ? lastSegment : to);

  // Lost (GPS dropout, a detour): look at the whole lap once.
  const float maxSquared = DELTA_MAX_OFFSET_M * DELTA_MAX_OFFSET_M;
  if (bestSquared > maxSquared) {
    bestSegment = -1;
    search(0, lastSegment);
    if (bestSquared > maxSquared) return false;
  }
  cursor_ = bestSegment;

  const TracePoint &a = reference[bestSegment];
  const TracePoint &b = reference[bestSegment + 1];
  float referenceMs = (float)a.timeMs + bestAlong * (float)(b.timeMs - a.timeMs);
  deltaMs = (int32_t)lapMs - (int32_t)referenceMs;
  return true;
}

}  // namespace core
