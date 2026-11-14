/*
  WheelStat - Alessandro Rota - Apache License 2.0

  Motorcycle telemetry on ESP32. The firmware is in src/ (entry point:
  src/App); what to edit for your build is in Config.h. The sketch folder
  must be called exactly "WheelStat".

  README.md for building and using it, docs/ARCHITETTURA.md for the code.
*/

#include "src/App.h"

void setup() { app::setup(); }

void loop() { app::loop(); }
