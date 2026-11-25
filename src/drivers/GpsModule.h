#pragma once

/*
  WheelStat - src/drivers/GpsModule.h
  ===========================================================================
  The GPS receiver up to the point where it sends NMEA at the right speed
  and rate. Reading the NMEA (src/Gps.cpp) is standard and the same for any
  receiver; what differs between makers is how to configure it, and that
  is all found out here, at boot, with nothing to set in Config.h:

    1. the baud rate, trying the usual ones until NMEA comes out;
    2. the family: a u-blox answers a UBX version request, and its
       hardware version says whether it wants the old configuration
       messages (M6/M8/M9) or the new keys (M10). Anything else gets the
       commands of the two other common families, CASIC (ATGM336H, Quectel
       L76K) and Airoha (Quectel LC76G/LC86G): each ignores the other's;
    3. faster serial (115200) and rate (10 Hz, 5 Hz if refused), then the
       rate actually obtained is MEASURED by counting the reports - the
       one number that matters, whatever the module did with the commands.

  The commands are framed in src/core/GpsCommands and checked on a PC.
  Only the u-blox M6/M8 path has met real hardware.
  ===========================================================================
*/

#include <Arduino.h>

// Hardware UART dedicated to the receiver (not the USB debug port).
extern HardwareSerial gpsSerial;

namespace gpsModule {

// Opens the UART and does all of the above. False if no NMEA ever came.
// Takes 1-3 s with a module, about 5 s without.
bool begin();

// What was found, for the boot checklist ("u-blox M8", "NMEA"...).
const char *name();

// Reports per second measured after the configuration (1 if unknown).
uint8_t measuredRateHz();

// How long without a report before the fix counts as lost: three and a
// half report intervals, so one late report does not drop it.
uint32_t fixTimeoutMs();

}  // namespace gpsModule
