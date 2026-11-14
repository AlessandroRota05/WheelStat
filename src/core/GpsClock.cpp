#include "GpsClock.h"

namespace core {

constexpr int32_t MS_PER_DAY = 24L * 60L * 60L * 1000L;

// The GPS step is believed only if it roughly agrees with millis. Reports
// that queued up while the loop was busy are parsed within the same
// millisecond but are 200 ms apart for the GPS: that is exactly the case to
// get right, and it is well inside the tolerance. A corrupt time field is
// not.
constexpr int32_t TOLERANCE_MS = 3000;

uint32_t GpsClock::onFix(int32_t timeOfDayMs, uint32_t localMs) {
  if (!started_) {
    started_ = true;
    clock_   = localMs;
  } else {
    int32_t localStep = (int32_t)(localMs - lastLocal_);
    int32_t gpsStep   = -1;
    if (timeOfDayMs >= 0 && lastTimeOfDay_ >= 0) {
      gpsStep = timeOfDayMs - lastTimeOfDay_;
      if (gpsStep < 0) gpsStep += MS_PER_DAY;  // midnight
    }

    int32_t disagreement = gpsStep - localStep;
    if (disagreement < 0) disagreement = -disagreement;
    clock_ += (gpsStep >= 0 && disagreement <= TOLERANCE_MS) ? gpsStep : localStep;
  }

  lastTimeOfDay_ = timeOfDayMs;
  lastLocal_     = localMs;
  return clock_;
}

uint32_t GpsClock::now(uint32_t localMs) const {
  if (!started_) return localMs;
  return clock_ + (localMs - lastLocal_);
}

}  // namespace core
