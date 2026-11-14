#pragma once

/*
  WheelStat - src/core/GpsClock.h
  ===========================================================================
  A monotonic millisecond clock that ticks with the GPS's own time of day.

  Lap times are differences between position reports. Stamping each report
  with millis() when it gets PARSED would put the main loop's delays into
  the lap time - and serving a web page can hold the loop for hundreds of
  milliseconds. The receiver stamps every report itself, to the hundredth,
  so that is the clock to use. millis() only bridges the gaps: before the
  first report, or when the time field is missing or makes no sense.
  ===========================================================================
*/

#include <stdint.h>

namespace core {

class GpsClock {
 public:
  // A report was parsed at `localMs` (millis) carrying `timeOfDayMs`,
  // milliseconds since UTC midnight, or negative if it has no valid time.
  // Returns the clock value for that report.
  uint32_t onFix(int32_t timeOfDayMs, uint32_t localMs);

  // The clock right now, carried forward from the last report with millis.
  uint32_t now(uint32_t localMs) const;

 private:
  bool     started_   = false;
  uint32_t clock_     = 0;
  int32_t  lastTimeOfDay_ = -1;
  uint32_t lastLocal_ = 0;
};

}  // namespace core
