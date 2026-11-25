#pragma once

/*
  WheelStat - src/drivers/WeatherSensor.h
  ===========================================================================
  Temperature/humidity sensor contract. There is no choice to make in
  Config.h: at boot every known sensor is looked for, and the first one
  that answers is used. The chips live one family per file in weather/.

  Only the DHT22 is tested on hardware. The others follow their datasheets
  and their arithmetic is checked on a PC (src/core/SensorMath): check them
  on first assembly.
  ===========================================================================
*/

#include <Arduino.h>

namespace weatherSensor {

// Looks for a sensor. False when none answers.
bool begin();

// The parameters arrive ALREADY FILLED with the last good values and only
// what was really read is overwritten: a sensor can deliver one valid
// quantity and not the other. False if nothing new was read.
//
// Most sensors measure in two phases - a call collects the measurement the
// previous call started, and starts the next - so nobody waits for a
// conversion. Values are therefore one interval old, which for the weather
// does not matter.
bool read(float &temperature, float &humidity);

// How often to call read(): a property of the sensor (the DHT22 repeats
// its last value if asked more often than every 2 s).
uint32_t intervalMs();

// Name shown on the boot checklist (about 11 characters at most), or "-"
// when nothing was found.
const char *name();

}  // namespace weatherSensor
