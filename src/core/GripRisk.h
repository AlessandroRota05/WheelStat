#pragma once

#include "../Tuning.h"

namespace core {

// Indicative 0-100 index: humidity above its threshold and temperature
// below its own raise the risk; warm, dry tarmac = 0.
inline float gripRisk(float temperature, float humidity) {
  float risk = 0.0f;
  if (humidity > GRIP_HUMIDITY_THRESHOLD)
    risk += (humidity - GRIP_HUMIDITY_THRESHOLD) * GRIP_HUMIDITY_WEIGHT;
  if (temperature < GRIP_TEMP_THRESHOLD)
    risk += (GRIP_TEMP_THRESHOLD - temperature) * GRIP_TEMP_WEIGHT;
  if (risk < 0.0f)   risk = 0.0f;
  if (risk > 100.0f) risk = 100.0f;
  return risk;
}

}  // namespace core
