#pragma once

/*
  WheelStat - src/Tuning.h
  ===========================================================================
  Behaviour tuning: thresholds, filters, timings and capacity limits. None
  of these change when different hardware is fitted - what does lives in
  Config.h. The values the rider can change from the website (event
  thresholds, lean alert, log rate) only have their DEFAULTS here: see
  src/core/SettingsRules.
  ===========================================================================
*/

#include <stdint.h>

constexpr float GRAVITY_MS2 = 9.81f;  // m/s^2 per G

// --- Filters -----------------------------------------------------------------
// Pothole filter: a value counts for records and events only if the level is
// HELD for the whole window. A pothole hit lasts 20-80 ms.
constexpr uint32_t POTHOLE_FILTER_MS = 150;

// Below this the pitch reads as zero: without it sensor noise would make
// page 3 flicker between WHEELIE and STOPPIE.
constexpr float PITCH_DEADBAND_DEG = 3.0f;

// --- Grip risk (see core::gripRisk) -----------------------------------------
constexpr float GRIP_HUMIDITY_THRESHOLD = 55.0f;  // no risk below this humidity
constexpr float GRIP_HUMIDITY_WEIGHT    = 1.3f;   // points per % above it
constexpr float GRIP_TEMP_THRESHOLD     = 20.0f;  // risk rises below this temperature
constexpr float GRIP_TEMP_WEIGHT        = 3.5f;   // points per degree below it

// Dynamic lean threshold of the page 0 alert: the higher the grip risk, the
// sooner the danger banner fires.
constexpr float LEAN_MAX_IDEAL_DEG       = 55.0f;  // default max lean on perfect tarmac
constexpr float LEAN_LOSS_PER_RISK_POINT = 0.35f;  // degrees "lost" per risk point

// --- Timings (ms) -------------------------------------------------------------
constexpr uint32_t DISPLAY_INTERVAL_MS = 100;    // 10 FPS
constexpr uint32_t SERIAL_INTERVAL_MS  = 5000;   // debug telemetry
constexpr uint32_t REC_BLINK_MS        = 500;    // "* REC" asterisk
constexpr uint32_t BUTTON_DEADTIME_MS  = 250;    // dead time after each press
constexpr uint32_t POPUP_MS            = 800;    // confirmation popups
constexpr uint32_t SPLASH_HOLD_MS      = 1500;   // time to read the boot checklist
constexpr uint32_t LOOP_PAUSE_MS       = 10;     // breathing room for the watchdog
constexpr uint32_t SUMMARY_TIMEOUT_MS  = 30000;  // auto-exit from blocking screens

// --- Session log --------------------------------------------------------------
// Samples per second written to flash. 24 bytes each: 3 Hz is ~260 KB per
// hour, about five hours on the 1.4 MB data partition.
constexpr uint8_t  LOG_RATE_DEFAULT_HZ = 3;
constexpr uint8_t  LOG_RATE_MIN_HZ     = 1;
constexpr uint8_t  LOG_RATE_MAX_HZ     = 10;

// Samples wait in RAM and reach the flash this often: a power cut loses at
// most this much, and the flash is not written three times a second.
constexpr uint32_t LOG_FLUSH_MS = 5000;

// --- Storage ------------------------------------------------------------------
constexpr int      MAX_SESSION_FILES = 9999;        // cap on LOG_n numbering
constexpr uint32_t MIN_FREE_BYTES    = 32 * 1024UL; // below this, recording does not start
// A running recording stops by itself when only this much is left, so the
// summary still fits and the filesystem never gets completely full.
constexpr uint32_t LOG_RESERVE_BYTES = 12 * 1024UL;
constexpr int      MAX_TRACKS        = 30;          // ~2 KB each on flash
constexpr int      MAX_SESSIONS_LISTED = 40;        // cap on the session lists of the website

// --- Lap timing ---------------------------------------------------------------
// A gate (finish line or checkpoint) is a line across the direction of
// travel, this wide on each side of the stored point.
constexpr float GATE_HALF_WIDTH_M = 20.0f;

// Below this speed a pass over a known gate is ignored: GPS noise on a bike
// parked on the line would otherwise cross it back and forth.
constexpr float GATE_MIN_SPEED_KMH = 5.0f;

// A gate learns its direction from the first pass, which needs a course
// over ground worth trusting; and after a standing start on the line that
// speed is only reached a few metres past it, hence the run-out.
constexpr float GATE_LEARN_SPEED_KMH = 15.0f;
constexpr float GATE_LEARN_RUNOUT_M  = 15.0f;

// Applies to EVERY sector, not just the whole lap: two checkpoints less than
// 15 s apart make the second pass be ignored.
constexpr uint32_t MIN_SECTOR_MS = 15000;
constexpr int      MAX_LAPS      = 50;      // laps kept per session (RAM arrays)

// Checkpoints are intermediate GPS gates that split the lap into sectors;
// the finish line is not counted. A track without checkpoints has the whole
// lap as its only sector.
constexpr int MAX_CHECKPOINTS = 2;
constexpr int MAX_SECTORS     = MAX_CHECKPOINTS + 1;

// Trace of a lap: position and time along it. It is the shape drawn on the
// website and the reference the live delta is measured against. The buffer
// thins itself out as the lap gets longer (see core::TraceRecorder), so a
// lap of any length fits. Must be even.
constexpr int MAX_TRACE_POINTS = 160;

// Further than this from the reference lap there is no meaningful delta
// (pit lane, another road).
constexpr float DELTA_MAX_OFFSET_M = 30.0f;
