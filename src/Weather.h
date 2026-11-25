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

// Reads the sensor and recomputes the grip risk. Call it every intervalMs().
void update();

const Reading &reading();

// How often update() is worth calling: a property of the sensor found.
uint32_t intervalMs();

}  // namespace weather
