#include "LapLogic.h"

namespace core {

void LapLogic::reset() {
  running_        = false;
  lapStart_       = 0;
  sectorStart_    = 0;
  nextCheckpoint_ = 0;
  lapCount_       = 0;
  bestLap_        = -1;
  for (int s = 0; s < MAX_SECTORS; s++) bestSectorMs_[s] = 0;
  traces_[0].clear();
  traces_[1].clear();
  hasClosedTrace_ = false;
}

bool LapLogic::takeCourseChanged() {
  bool changed = courseChanged_;
  courseChanged_ = false;
  return changed;
}

const Gate &LapLogic::nextGate(bool &isFinish) const {
  isFinish = !running_ || nextCheckpoint_ >= course_.checkpointCount;
  return isFinish ? course_.finish : course_.checkpoints[nextCheckpoint_];
}

int LapLogic::closedTrace(const TracePoint *&points) const {
  const TraceRecorder &closed = traces_[1 - openTrace_];
  points = closed.points();
  return hasClosedTrace_ ? closed.count() : 0;
}

void LapLogic::closeSector(int sector, uint32_t when) {
  uint32_t duration = when - sectorStart_;
  if (lapCount_ < MAX_LAPS) sectorMs_[lapCount_][sector] = duration;
  if (bestSectorMs_[sector] == 0 || duration < bestSectorMs_[sector])
    bestSectorMs_[sector] = duration;
}

LapEvent LapLogic::onFix(const GpsSample &fix) {
  if (!course_.finishSet) {
    havePrev_ = false;
    return LapEvent::None;
  }
  if (!havePrev_) {
    prev_     = fix;
    havePrev_ = true;
    return LapEvent::None;
  }
  const GpsSample prev = prev_;
  prev_ = fix;

  bool isFinish;
  nextGate(isFinish);
  Gate &gate = isFinish ? course_.finish : course_.checkpoints[nextCheckpoint_];

  const Crossing crossing = detectCrossing(gate, prev, fix);
  const bool     reversed = crossing.pass == Pass::Reversed;
  const uint32_t when =
    prev.timeMs + (uint32_t)((float)(fix.timeMs - prev.timeMs) * crossing.fraction);

  // A reversed pass is exempt from the minimum time: it is not a second
  // pass of the same lap, it is the discovery that the first one was not
  // the start of a lap at all.
  bool counts = crossing.pass != Pass::None;
  if (counts && running_ && !reversed && when - sectorStart_ < MIN_SECTOR_MS) counts = false;

  if (!counts) {
    if (running_) openTrace().add(fix.lat, fix.lon, fix.timeMs - lapStart_);
    return LapEvent::None;
  }

  if (crossing.pass == Pass::Learned || reversed) {
    gate.headingDeg = fix.courseDeg;
    gate.confidence = GATE_PROVISIONAL;
    courseChanged_  = true;
  } else if (gate.confidence == GATE_PROVISIONAL) {
    gate.confidence = GATE_CONFIRMED;
    courseChanged_  = true;
  }

  if (!isFinish) {
    // Intermediate checkpoint: closes the sector only, the lap stays open.
    closeSector(nextCheckpoint_, when);
    sectorStart_ = when;
    nextCheckpoint_++;
    openTrace().add(fix.lat, fix.lon, fix.timeMs - lapStart_);
    return LapEvent::Sector;
  }

  // Finish line: closes the last sector and the lap - unless this is the
  // very first pass, or one that turned the line around: those only arm
  // the stopwatch.
  LapEvent event = LapEvent::Started;
  if (running_ && !reversed) {
    closeSector(course_.checkpointCount, when);

    event = LapEvent::Lap;
    if (lapCount_ < MAX_LAPS) {
      uint32_t duration = when - lapStart_;
      if (bestLap_ < 0 || duration < lapMs_[bestLap_]) {
        bestLap_ = lapCount_;
        event    = LapEvent::BestLap;
      }
      lapMs_[lapCount_++] = duration;
    }

    openTrace_      = 1 - openTrace_;
    hasClosedTrace_ = true;
  }

  // The same pass opens the stopwatch for the next lap.
  lapStart_       = when;
  sectorStart_    = when;
  running_        = true;
  nextCheckpoint_ = 0;
  openTrace().clear();
  openTrace().add(fix.lat, fix.lon, fix.timeMs - when);
  return event;
}

}  // namespace core
