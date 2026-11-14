#pragma once

/*
  WheelStat - src/Recorder.h
  ===========================================================================
  Session recording to flash: a few samples a second (the rate is a
  setting) in a compact binary file, and a summary appended when the
  recording stops. Layout in src/core/SessionCodec.h.
  ===========================================================================
*/

#include <Arduino.h>

namespace recorder {

// Picks the first free LOG_n.BIN, writes its header and clears the session
// statistics. False (with a reason on the serial monitor) if it cannot.
bool start();

// Writes what is still in RAM and the summary, then updates the all-time
// records.
void stop();

// Takes the samples that are due. Call every loop. True if the storage ran
// out and the recording had to be stopped here: stop() has already run.
bool update();

bool        isRecording();
const char *fileName();        // e.g. "/LOG_12.BIN"
uint32_t    minutesLogged();
uint32_t    durationSeconds(); // of the running session, or of the last one

}  // namespace recorder
