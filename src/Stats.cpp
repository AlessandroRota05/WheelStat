#include "Stats.h"

#include "Gps.h"
#include "Motion.h"
#include "Settings.h"
#include "core/PeakFilter.h"

namespace stats {

static core::PeakFilter filter;
static bool intervalFresh = false;  // a window closed since the last take

void reset() {
  filter.reset(millis());
  intervalFresh = false;
}

void sample() {
  const motion::State &m = motion::state();

  // Instantaneous channel values, all positive (the sign picks the channel).
  float now[CHANNEL_COUNT];
  now[CH_LEAN_RIGHT] = (m.lean >= 0) ?  m.lean : 0.0f;
  now[CH_LEAN_LEFT]  = (m.lean <  0) ? -m.lean : 0.0f;
  now[CH_WHEELIE]    = m.wheelie;
  now[CH_STOPPIE]    = m.stoppie;
  now[CH_GLAT_RIGHT] = (m.gLat >= 0) ?  m.gLat : 0.0f;
  now[CH_GLAT_LEFT]  = (m.gLat <  0) ? -m.gLat : 0.0f;
  now[CH_G_ACCEL]    = (m.gLong >= 0) ?  m.gLong : 0.0f;
  now[CH_G_BRAKE]    = (m.gLong <  0) ? -m.gLong : 0.0f;
  // The GPS updates far slower than the window, so the filter is a no-op
  // for this channel - but staying on the same table means no special case
  // anywhere else (records, log, pages).
  now[CH_SPEED]      = gps::fix().speedKmh;

  if (filter.sample(millis(), now, settings::eventThresholds())) intervalFresh = true;
}

const float *sessionMax() { return filter.sessionMax(); }

bool takeIntervalMax(float *out) {
  if (!intervalFresh) return false;
  for (int c = 0; c < CHANNEL_COUNT; c++) out[c] = filter.intervalMax()[c];
  filter.clearIntervalMax();
  intervalFresh = false;
  return true;
}

uint16_t eventCount(uint8_t event) { return filter.eventCount(event); }

}  // namespace stats
