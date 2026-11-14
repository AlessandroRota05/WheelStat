#pragma once

/*
  WheelStat - src/Buttons.h
  ===========================================================================
  The four push buttons: edge detection and the dead time after each press.
  ===========================================================================
*/

#include <Arduino.h>

namespace buttons {

enum class Button : uint8_t { None, Up, Down, Ok, Log };

void begin();

// The button that was just pressed, at most one per call (Up wins over
// Down, then Ok, then Log). Presses during the dead time are discarded.
// The caller ends its handling with holdOff().
Button poll();

// Restarts the dead time. Besides debouncing, it prevents accidental combined
// presses - and after a blocking screen it stops the key that closed it from
// also driving the menu.
void holdOff();

// Raw edges, ignoring the dead time: for the blocking screens.
bool anyPressed();
bool okPressed();

}  // namespace buttons
