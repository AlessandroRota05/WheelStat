#pragma once

/*
  WheelStat - src/drivers/GpsModule.h
  ===========================================================================
  GPS receiver contract: the UART it hangs on and the settings it is sent at
  boot. That is the only module-specific part - READING (src/Gps.cpp) is
  standard NMEA and works with any receiver.

  Three drivers, because u-blox changed mechanism along the way:
    GPS_UBLOX         M6/M8/M9   CFG-MSG and CFG-RATE messages
    GPS_UBLOX_M10     M10        CFG-VALSET keys (the old ones are gone)
    GPS_NMEA_GENERIC  others     nothing: factory settings
  An M8 accepts both, an M10 only the new one, an M6 only the old one.

  Only the M6/M8 branch is tested on hardware.
  ===========================================================================
*/

#include <Arduino.h>

// Hardware UART dedicated to the receiver (not the USB debug port).
extern HardwareSerial gpsSerial;

namespace gpsModule {

// Sends the module its settings. May do nothing. The UART must be open.
void configure();

// Name shown on the boot checklist (about 11 characters at most).
const char *name();

}  // namespace gpsModule
