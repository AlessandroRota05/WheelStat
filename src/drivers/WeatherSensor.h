#pragma once

/*
  WheelStat - src/drivers/WeatherSensor.h
  ===========================================================================
  Temperature/humidity sensor contract. The chip is picked in Config.h
  (WEATHER_*); exactly one implementation is compiled.

  Only the DHT22 is tested on hardware. The others compile and honour the
  contract: check them on first assembly (I2C address first).
  ===========================================================================
*/

#include <Arduino.h>

namespace weatherSensor {

// False when the sensor is not there: I2C ones because nothing answers on
// the address, the DHT22 - which has no address - because it is actually
// read and delivers NaN.
bool begin();

// The parameters arrive ALREADY FILLED with the last good values and only
// what was really read is overwritten: a sensor can deliver one valid
// quantity and NaN for the other. False if nothing could be read.
bool read(float &temperature, float &humidity);

// Name shown on the boot checklist (about 11 characters at most).
const char *name();

}  // namespace weatherSensor
