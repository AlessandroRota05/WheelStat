// u-blox receivers: NEO-6M, NEO-7M, NEO-M8N, NEO-M9N, MAX-M10S and the
// boards built on them (Beitian BN-220/BN-880 are M8).
//
// u-blox changed configuration mechanism along the way. Up to the M9 there
// are dedicated messages (CFG-MSG, CFG-RATE, CFG-PRT); the M10 dropped them
// for "keys" set with CFG-VALSET. The M8 and M9 accept the old ones, the M10
// only the new ones, the M6 only the old ones - so the hardware version is
// asked first (MON-VER) and the path follows from it.
//
// Settings go to RAM only: the module returns to factory settings when it
// loses power, and the next boot configures it again.

#include "GpsLink.h"

namespace gpsModule {

// --- Old mechanism (M6 - M9) -----------------------------------------------------

// UBX-CFG-MSG with rate 0 turns one NMEA sentence off. GGA and RMC stay on:
// the only two the firmware reads.
static void legacySentenceOff(uint8_t sentenceId) {
  const uint8_t payload[3] = {0xF0, sentenceId, 0x00};
  sendUbx(0x06, 0x01, payload, 3);
  waitAck(0x06, 0x01, 300);
}

// UBX-CFG-PRT for UART1: 8N1, UBX+NMEA in, UBX+NMEA out, new speed. The
// module switches at once, so its ACK may come at either speed: ignored,
// followBaud() checks the result instead.
static void legacyBaud(uint32_t baud) {
  const uint8_t payload[20] = {
    0x01, 0x00, 0x00, 0x00,                                   // port 1, reserved, txReady
    0xD0, 0x08, 0x00, 0x00,                                   // mode: 8 bits, no parity, 1 stop
    (uint8_t)baud, (uint8_t)(baud >> 8), (uint8_t)(baud >> 16), (uint8_t)(baud >> 24),
    0x07, 0x00, 0x03, 0x00,                                   // protocols in / out
    0x00, 0x00, 0x00, 0x00,                                   // flags, reserved
  };
  sendUbx(0x06, 0x00, payload, 20);
}

// UBX-CFG-RATE: measurement period in ms, one solution per measurement,
// GPS time. True if the module accepted it.
static bool legacyRate(uint16_t periodMs) {
  const uint8_t payload[6] = {(uint8_t)periodMs, (uint8_t)(periodMs >> 8), 1, 0, 1, 0};
  sendUbx(0x06, 0x08, payload, 6);
  return waitAck(0x06, 0x08, 400) == 1;
}

// --- New mechanism (M10) -----------------------------------------------------------

constexpr uint32_t CFG_RATE_MEAS          = 0x30210001;   // U2, ms
constexpr uint32_t CFG_UART1_BAUDRATE     = 0x40520001;   // U4
constexpr uint32_t CFG_MSGOUT_NMEA_GLL    = 0x209100CA;   // U1, UART1
constexpr uint32_t CFG_MSGOUT_NMEA_GSA    = 0x209100C0;
constexpr uint32_t CFG_MSGOUT_NMEA_GSV    = 0x209100C5;
constexpr uint32_t CFG_MSGOUT_NMEA_VTG    = 0x209100B1;

// UBX-CFG-VALSET to RAM: version, layer, 2 reserved, key, value. The size
// of the value is in the key itself (bits 28-30: 2 = 1 byte, 3 = 2, 4 = 4).
static bool setKey(uint32_t key, uint32_t value, bool waitForAck = true) {
  uint8_t payload[12] = {0x00, 0x01, 0x00, 0x00,
                         (uint8_t)key, (uint8_t)(key >> 8), (uint8_t)(key >> 16), (uint8_t)(key >> 24)};
  uint8_t sizeCode = (key >> 28) & 0x07;
  uint8_t bytes = sizeCode == 2 ? 1 : sizeCode == 3 ? 2 : 4;
  for (uint8_t i = 0; i < bytes; i++) payload[8 + i] = (uint8_t)(value >> (8 * i));
  sendUbx(0x06, 0x8A, payload, 8 + bytes);
  return !waitForAck || waitAck(0x06, 0x8A, 400) == 1;
}

// --- Both ----------------------------------------------------------------------

// MON-VER answers 30 bytes of software version and 10 of hardware version,
// "0008000000" for an M8: its first four hex digits are the generation.
const char *ubloxConfigure(uint32_t &baud) {
  drain();
  sendUbx(0x0A, 0x04, nullptr, 0);
  uint8_t version[40];
  if (waitUbx(0x0A, 0x04, version, sizeof(version), 1200) < 40) return nullptr;

  char generation[5];
  memcpy(generation, version + 30, 4);
  generation[4] = '\0';
  long gen = strtol(generation, nullptr, 16);

  const char *name;
  switch (gen) {
    case 0x0004: name = "u-blox 6";   break;
    case 0x0007: name = "u-blox 7";   break;
    case 0x0008: name = "u-blox M8";  break;
    case 0x0019: name = "u-blox M9";  break;
    case 0x000A: name = "u-blox M10"; break;
    default:     name = "u-blox";     break;
  }

  if (gen == 0x000A) {
    setKey(CFG_MSGOUT_NMEA_GLL, 0);
    setKey(CFG_MSGOUT_NMEA_GSA, 0);
    setKey(CFG_MSGOUT_NMEA_GSV, 0);
    setKey(CFG_MSGOUT_NMEA_VTG, 0);
    if (baud < 115200) {
      setKey(CFG_UART1_BAUDRATE, 115200, false);
      if (followBaud(115200, baud)) baud = 115200;
    }
    // 10 Hz only when the line can carry it; the M10 does 10 Hz on all
    // constellations, so a refusal is not expected.
    if (baud < 38400 || !setKey(CFG_RATE_MEAS, 100)) setKey(CFG_RATE_MEAS, 200);
  } else {
    // Free the bandwidth first, then raise the rate: the other way round
    // the module would send the whole default set at the higher rate for a
    // few cycles and saturate the line.
    legacySentenceOff(0x01);   // GLL
    legacySentenceOff(0x02);   // GSA
    legacySentenceOff(0x03);   // GSV
    legacySentenceOff(0x05);   // VTG
    if (baud < 115200) {
      legacyBaud(115200);
      if (followBaud(115200, baud)) baud = 115200;
    }
    // The NEO-6M stops at 5 Hz and says no to 10: then 5.
    if (baud < 38400 || !legacyRate(100)) legacyRate(200);
  }
  return name;
}

}  // namespace gpsModule
