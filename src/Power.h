#pragma once

// WheelStat - src/Power.h
// The bike's switched 12 V, read through a divider (POWER_SENSE_PIN in
// Config.h). Its one job is to say when the key has been turned off, so the
// recording can be closed while the capacitor still keeps the board alive.
// Without POWER_SENSE_PIN everything here does nothing.

#include <Arduino.h>

namespace power {

void begin();

// True only for the sensing build.
bool sensing();

// True ONCE, when the voltage has stayed below POWER_OFF_VOLTS for a few
// readings in a row. It re-arms when the voltage comes back.
bool keyTurnedOff();

// Last reading of the 12 V line, NAN without sensing.
float volts();

}  // namespace power
