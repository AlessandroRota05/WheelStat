#pragma once

// WheelStat - src/core/GpsCommands.h
// The framing of the commands sent to GPS receivers: NMEA-style proprietary
// sentences ("$PCAS...", "$PAIR...") with their XOR checksum, and u-blox
// UBX frames with their Fletcher checksum. Built here so they can be
// checked on a PC against the manufacturers' examples.

#include <stddef.h>
#include <stdint.h>

namespace core {

// "$" + body + "*" + two hex digits + CR LF, into out. Returns the length,
// 0 if it does not fit.
size_t nmeaSentence(const char *body, char *out, size_t size);

// A complete UBX frame (sync, class, id, length, payload, checksum) into
// out. Returns the length, 0 if it does not fit.
size_t ubxFrame(uint8_t msgClass, uint8_t msgId, const uint8_t *payload, uint16_t length,
                uint8_t *out, size_t size);

}  // namespace core
