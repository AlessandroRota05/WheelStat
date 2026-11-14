#include "Weather.h"

#include "core/GripRisk.h"
#include "drivers/WeatherSensor.h"

namespace weather {

// Neutral starting values (20 degrees / 50 %, risk 0) until the first read.
static Reading current = { false, 20.0f, 50.0f, 0.0f };

bool begin() {
  current.sensorOk = weatherSensor::begin();
  return current.sensorOk;
}

void update() {
  // The driver starts from the last good values and overwrites only what it
  // really reads, so a bad humidity sample does not take a valid
  // temperature down with it.
  float temperature = current.temperature;
  float humidity    = current.humidity;

  if (current.sensorOk && weatherSensor::read(temperature, humidity)) {
    current.temperature = temperature;
    current.humidity    = humidity;
  }

  // Recomputed even after a failed read or with no sensor, so the risk
  // always matches the values on screen.
  current.gripRisk = core::gripRisk(current.temperature, current.humidity);
}

const Reading &reading() { return current; }

}  // namespace weather
