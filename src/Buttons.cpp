#include "Buttons.h"

#include "../Config.h"
#include "Tuning.h"

namespace buttons {

struct Key {
  uint8_t pin;
  int     previous;  // previous level, for edge detection
};

// Same order as the Button enum, minus None.
static Key keys[] = {
  { PIN_BTN_UP,   HIGH },
  { PIN_BTN_DOWN, HIGH },
  { PIN_BTN_OK,   HIGH },
  { PIN_BTN_LOG,  HIGH },
};
constexpr int KEY_COUNT = sizeof(keys) / sizeof(keys[0]);
constexpr int KEY_OK    = 2;

static uint32_t lastPress = 0;

// True only in the very cycle the button goes from HIGH to LOW: holding it
// generates nothing more.
static bool pressedEdge(Key &key) {
  int level = digitalRead(key.pin);
  bool pressed = (level == LOW && key.previous == HIGH);
  key.previous = level;
  return pressed;
}

void begin() {
  // Buttons to GND with the internal pull-up: they read HIGH at rest.
  for (int i = 0; i < KEY_COUNT; i++) pinMode(keys[i].pin, INPUT_PULLUP);
}

Button poll() {
  // ALL four edges are sampled on every call, so every previous level stays
  // up to date even while the dead time discards the presses.
  int pressed = -1;
  for (int i = KEY_COUNT - 1; i >= 0; i--)
    if (pressedEdge(keys[i])) pressed = i;

  if (pressed < 0 || millis() - lastPress < BUTTON_DEADTIME_MS) return Button::None;
  return (Button)(pressed + 1);
}

void holdOff() { lastPress = millis(); }

bool anyPressed() {
  bool any = false;
  for (int i = 0; i < KEY_COUNT; i++) any |= pressedEdge(keys[i]);
  return any;
}

bool okPressed() { return pressedEdge(keys[KEY_OK]); }

}  // namespace buttons
