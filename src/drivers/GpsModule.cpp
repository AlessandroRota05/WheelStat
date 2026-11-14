#include "GpsModule.h"

#include "../../Config.h"

// Which UART depends on the chip: the C3 and S2 have two instead of three,
// and 0 is always taken by the serial monitor.
HardwareSerial gpsSerial(GPS_UART_NUM);

// ===========================================================================
// Shared by both u-blox drivers
// ===========================================================================
#if defined(GPS_UBLOX) || defined(GPS_UBLOX_M10)

// UBX frame with the Fletcher-8 checksum computed rather than copied from a
// tutorial, so it works for any command.
static void sendUbx(uint8_t msgClass, uint8_t msgId, const uint8_t *payload, uint16_t length) {
  uint8_t header[4] = {
    msgClass, msgId, (uint8_t)(length & 0xFF), (uint8_t)(length >> 8)
  };

  uint8_t ckA = 0, ckB = 0;
  for (uint8_t i = 0; i < sizeof(header); i++) { ckA += header[i];  ckB += ckA; }
  for (uint16_t i = 0; i < length; i++)        { ckA += payload[i]; ckB += ckA; }

  gpsSerial.write(0xB5);
  gpsSerial.write(0x62);
  gpsSerial.write(header, sizeof(header));
  gpsSerial.write(payload, length);
  gpsSerial.write(ckA);
  gpsSerial.write(ckB);
}
#endif

// ===========================================================================
// M6 / M8 / M9 - dedicated configuration messages
// ===========================================================================
#ifdef GPS_UBLOX

namespace gpsModule {

// UBX-CFG-MSG with rate 0 turns an NMEA sentence off. sentenceId is the ID
// in the standard NMEA class (0xF0): GLL=0x01, GSA=0x02, GSV=0x03, VTG=0x05.
// GGA (0x00) and RMC (0x04) stay on: the only two TinyGPS++ needs here.
static void disableNmeaSentence(uint8_t sentenceId) {
  uint8_t payload[3] = { 0xF0, sentenceId, 0x00 };
  sendUbx(0x06, 0x01, payload, sizeof(payload));
}

// Free the bandwidth first, then raise the rate: the other way round the
// module would try to send the whole default set at the higher rate for a
// few cycles and saturate the 9600 baud line (the sums are in Config.h).
//
// ACKs are deliberately not read: with no module the commands go nowhere
// without errors or waits, and gps::update() validates the fix by itself.
void configure() {
  disableNmeaSentence(0x01); delay(50);  // GLL
  disableNmeaSentence(0x02); delay(50);  // GSA
  disableNmeaSentence(0x03); delay(50);  // GSV
  disableNmeaSentence(0x05); delay(50);  // VTG

  uint16_t measureMs = 1000 / GPS_RATE_HZ;
  uint8_t payload[6] = {
    (uint8_t)(measureMs & 0xFF), (uint8_t)(measureMs >> 8),  // measRate, ms
    1, 0,                                                     // navRate: 1 solution per measurement
    1, 0                                                      // timeRef: GPS time
  };
  sendUbx(0x06, 0x08, payload, sizeof(payload));  // UBX-CFG-RATE
}

const char *name() { return "u-blox"; }

}  // namespace gpsModule
#endif

// ===========================================================================
// M10 - CFG-VALSET keys
// ===========================================================================
#ifdef GPS_UBLOX_M10

namespace gpsModule {

// Each setting has a 32-bit key that also encodes its size: 0x20... = 1 byte
// (NMEA sentence rates), 0x30... = 2 bytes (measurement period).
//
// CFG-RATE-MEAS is documented everywhere. The four NMEA keys come from the
// M10 interface manual and are NOT field-verified: if one were wrong the
// module answers NAK and sendKey() prints the setting's name. That is why
// this driver reads ACKs and the M8 one does not - otherwise a mistake
// would look like a GPS silently stuck at 1 Hz.
constexpr uint32_t CFG_RATE_MEAS       = 0x30210001;
constexpr uint32_t CFG_MSGOUT_NMEA_GLL = 0x209100CA;
constexpr uint32_t CFG_MSGOUT_NMEA_GSA = 0x209100C0;
constexpr uint32_t CFG_MSGOUT_NMEA_GSV = 0x209100C5;
constexpr uint32_t CFG_MSGOUT_NMEA_VTG = 0x209100B1;

// Looks for UBX-ACK-ACK (class 0x05, id 0x01) or UBX-ACK-NAK (0x05, 0x00)
// in the incoming stream, which also carries NMEA sentences meanwhile.
// Neither the checksum nor the acknowledged command is checked: keys are
// sent one at a time, so the first answer is necessarily for the last one.
// Timeout = module missing, or not speaking UBX.
static bool waitForAck(uint32_t timeoutMs) {
  const uint32_t start = millis();
  uint8_t matched = 0;  // header bytes recognised so far

  while (millis() - start < timeoutMs) {
    if (!gpsSerial.available()) continue;
    uint8_t b = gpsSerial.read();
    switch (matched) {
      case 0: matched = (b == 0xB5) ? 1 : 0; break;
      case 1: matched = (b == 0x62) ? 2 : 0; break;
      case 2: matched = (b == 0x05) ? 3 : 0; break;
      case 3: return (b == 0x01);
    }
  }
  return false;
}

// UBX-CFG-VALSET payload: version, layers, 2 reserved, key (4 bytes
// little-endian), value. Layer 0x01 = RAM only, so the module can be reused
// elsewhere with no surprises after a power cycle.
static bool sendKey(uint32_t key, uint16_t value, uint8_t valueBytes, const char *label) {
  uint8_t payload[10];
  uint8_t n = 0;
  payload[n++] = 0x00;
  payload[n++] = 0x01;
  payload[n++] = 0x00;
  payload[n++] = 0x00;
  for (uint8_t i = 0; i < 4; i++) payload[n++] = (uint8_t)(key >> (8 * i));
  payload[n++] = (uint8_t)(value & 0xFF);
  if (valueBytes == 2) payload[n++] = (uint8_t)(value >> 8);

  sendUbx(0x06, 0x8A, payload, n);
  if (waitForAck(1000)) return true;

  Serial.print(F("  GPS M10: rejected or no answer -> "));
  Serial.println(label);
  return false;
}

void configure() {
  bool ok = true;
  ok &= sendKey(CFG_MSGOUT_NMEA_GLL, 0, 1, "GLL off");
  ok &= sendKey(CFG_MSGOUT_NMEA_GSA, 0, 1, "GSA off");
  ok &= sendKey(CFG_MSGOUT_NMEA_GSV, 0, 1, "GSV off");
  ok &= sendKey(CFG_MSGOUT_NMEA_VTG, 0, 1, "VTG off");
  ok &= sendKey(CFG_RATE_MEAS, 1000 / GPS_RATE_HZ, 2, "rate");

  if (!ok) {
    Serial.println(F("  GPS M10: configuration INCOMPLETE, the module stays at 1 Hz"));
    Serial.println(F("  and the fix will be intermittent. If this is not an M10, use"));
    Serial.println(F("  GPS_UBLOX (M6/M8/M9) or GPS_NMEA_GENERIC."));
  }
}

const char *name() { return "u-blox M10"; }

}  // namespace gpsModule
#endif

// ===========================================================================
// Non-u-blox modules
// ===========================================================================
#ifdef GPS_NMEA_GENERIC

namespace gpsModule {

// Empty on purpose: every non-u-blox family has proprietary, often
// undocumented commands, so the module is left as it is. It stays at 1 Hz
// instead of 5: hardly noticeable on the road, but at 100 km/h that is 28 m
// between samples instead of 5.6, which lap timing does feel.
// GPS_FIX_TIMEOUT_MS in Config.h is already widened to match.
void configure() {}

const char *name() { return "NMEA"; }

}  // namespace gpsModule
#endif
