#include "LapTimer.h"

#include "Gps.h"

namespace laps {

static core::LapLogic  logic;
static core::GpsSample lastReport = {};
static uint32_t        reports = 0;

void setCourse(const Course &course) { logic.setCourse(course); }

const Course &course() { return logic.course(); }

bool takeCourseChanged() { return logic.takeCourseChanged(); }

void resetLaps() { logic.reset(); }

Event update() {
  Event highest = Event::None;

  core::GpsSample report;
  while (gps::nextSample(report)) {
    Event event = logic.onFix(report);
    if (event > highest) highest = event;
    lastReport = report;
    reports++;
  }
  return highest;
}

bool nextTarget(double &lat, double &lon) {
  bool isFinish;
  const Gate &gate = logic.nextGate(isFinish);
  lat = gate.lat;
  lon = gate.lon;
  return isFinish;
}

bool lapRunning() { return logic.running(); }

uint32_t currentLapMs() {
  return logic.running() ? (gps::clockMs() - logic.lapStartMs()) : 0;
}

int lapCount() { return logic.lapCount(); }

uint32_t lapTime(int lap) { return logic.lapMs(lap); }

int bestLapIndex() { return logic.bestLap(); }

uint32_t bestLapMs() { return logic.bestLap() >= 0 ? logic.lapMs(logic.bestLap()) : 0; }

uint32_t lastLapMs() { return logic.lapCount() > 0 ? logic.lapMs(logic.lapCount() - 1) : 0; }

int sectorCount() { return logic.sectorCount(); }

uint32_t sectorTime(int lap, int sector) { return logic.sectorMs(lap, sector); }

uint32_t bestSectorMs(int sector) { return logic.bestSectorMs(sector); }

int closedLapTrace(const core::TracePoint *&points) { return logic.closedTrace(points); }

uint32_t reportCount() { return reports; }

bool lastPosition(double &lat, double &lon, uint32_t &lapMs) {
  if (!logic.running() || reports == 0) return false;
  lat   = lastReport.lat;
  lon   = lastReport.lon;
  lapMs = lastReport.timeMs - logic.lapStartMs();
  return true;
}

}  // namespace laps
