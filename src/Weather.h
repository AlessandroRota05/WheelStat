#pragma once

/*
  WheelStat - src/Weather.h
  ===========================================================================
  Air temperature and humidity, and the grip risk index derived from them.
  ===========================================================================
*/

#include <Arduino.h>

namespace weather {

struct Reading {
  bool  sensorOk;
  float temperature;  // deg C, last good value
  float humidity;     // %, last good value
  float gripRisk;     // 0-100 %
};

bool begin();

// Reads the sensor and recomputes the grip risk. Call it every
// WEATHER_INTERVAL_MS (Config.h).
void update();

const Reading &reading();

}  // namespace weather
