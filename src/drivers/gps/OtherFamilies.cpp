// Receivers that are not u-blox. The two families common on cheap boards
// both take NMEA-style proprietary sentences:
//   CASIC   "$PCAS.."  ZhongKeWei ATGM336H, Quectel L76K, many clones
//   Airoha  "$PAIR.."  Quectel LC76G / LC86G / LC29H
// A module ignores the other family's sentences (they are just unknown
// NMEA to it), so both sets are sent and whichever applies takes effect.
// Whether it worked is not assumed: GpsModule.cpp measures the rate after.
//
// Any other NMEA module keeps its factory settings - usually 1 Hz: lap
// times then come from positions 28 m apart at 100 km/h instead of 2.8 m.

#include "GpsLink.h"

namespace gpsModule {

void otherFamiliesConfigure(uint32_t &baud) {
  // Only GGA and RMC: the bandwidth for a higher rate.
  sendNmea("PCAS03,1,0,0,0,1,0,0,0,0,0,,,0,0");   // CASIC: GGA, GLL, GSA, GSV, RMC, VTG...
  sendNmea("PAIR062,1,0");                         // Airoha: GLL off
  sendNmea("PAIR062,2,0");                         //         GSA off
  sendNmea("PAIR062,3,0");                         //         GSV off
  sendNmea("PAIR062,5,0");                         //         VTG off

  if (baud < 115200) {
    sendNmea("PCAS01,5");              // CASIC: speed code 5 = 115200
    sendNmea("PAIR864,0,0,115200");
    if (followBaud(115200, baud)) baud = 115200;
  }

  // Fix interval in ms. At 9600 baud 10 Hz would not fit even with two
  // sentences, so 5.
  if (baud >= 38400) {
    sendNmea("PCAS02,100");
    sendNmea("PAIR050,100");
  } else {
    sendNmea("PCAS02,200");
    sendNmea("PAIR050,200");
  }
}

}  // namespace gpsModule
