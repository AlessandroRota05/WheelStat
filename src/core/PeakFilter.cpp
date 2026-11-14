#include "PeakFilter.h"

#include "../Tuning.h"

namespace core {

void PeakFilter::reset(uint32_t nowMs) {
  for (int c = 0; c < CHANNEL_COUNT; c++) {
    intervalMax_[c] = 0.0f;
    sessionMax_[c]  = 0.0f;
  }
  windowOpen_  = false;
  windowStart_ = nowMs;  // the next window starts whole
  for (int e = 0; e < EVENT_COUNT; e++) {
    eventCount_[e]  = 0;
    eventActive_[e] = false;
  }
}

void PeakFilter::clearIntervalMax() {
  for (int c = 0; c < CHANNEL_COUNT; c++) intervalMax_[c] = 0.0f;
}

void PeakFilter::countEvent(uint8_t event, float level, float threshold) {
  if (!eventActive_[event] && level >= threshold) {
    eventActive_[event] = true;
    eventCount_[event]++;
  } else if (eventActive_[event] && level < threshold * EVENT_REARM_RATIO) {
    eventActive_[event] = false;
  }
}

bool PeakFilter::sample(uint32_t nowMs, const float *values, const float *eventThresholds) {
  if (!windowOpen_) {
    for (int c = 0; c < CHANNEL_COUNT; c++) windowMin_[c] = values[c];
    windowOpen_ = true;
  } else {
    for (int c = 0; c < CHANNEL_COUNT; c++)
      if (values[c] < windowMin_[c]) windowMin_[c] = values[c];
  }

  if (nowMs - windowStart_ < POTHOLE_FILTER_MS) return false;
  windowStart_ = nowMs;
  windowOpen_  = false;  // the next sample opens a new window

  for (int c = 0; c < CHANNEL_COUNT; c++) {
    if (windowMin_[c] > intervalMax_[c]) intervalMax_[c] = windowMin_[c];
    if (windowMin_[c] > sessionMax_[c])  sessionMax_[c]  = windowMin_[c];
  }

  // Working on the filter output means a sharp hit on a pothole does not
  // count as hard braking or a wheelie.
  countEvent(EV_WHEELIE, windowMin_[CH_WHEELIE], eventThresholds[EV_WHEELIE]);
  countEvent(EV_STOPPIE, windowMin_[CH_STOPPIE], eventThresholds[EV_STOPPIE]);
  // For lean the more tilted side counts: right and left are one event.
  float lean = windowMin_[CH_LEAN_RIGHT] > windowMin_[CH_LEAN_LEFT] ? windowMin_[CH_LEAN_RIGHT]
                                                                    : windowMin_[CH_LEAN_LEFT];
  countEvent(EV_LEAN,       lean,                    eventThresholds[EV_LEAN]);
  countEvent(EV_HARD_BRAKE, windowMin_[CH_G_BRAKE],  eventThresholds[EV_HARD_BRAKE]);
  countEvent(EV_HARD_ACCEL, windowMin_[CH_G_ACCEL],  eventThresholds[EV_HARD_ACCEL]);
  return true;
}

}  // namespace core
