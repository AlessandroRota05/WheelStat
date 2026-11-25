#include "GpsModule.h"

#include "../../Config.h"
#include "../core/GpsCommands.h"
#include "gps/GpsLink.h"

// Which UART depends on the chip: the C3 and S2 have two instead of three,
// and 0 is always taken by the serial monitor.
HardwareSerial gpsSerial(GPS_UART_NUM);

namespace gpsModule {

static const char *foundName = "-";
static uint8_t     rateHz = 1;

// ===========================================================================
// The line
// ===========================================================================

void open(uint32_t baud) {
  gpsSerial.begin(baud, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  drain();
}

void drain() {
  while (gpsSerial.available()) gpsSerial.read();
}

// Opening the UART proves nothing: it opens without errors even into thin
// air. The only proof of a module is bytes coming out of it, and a powered
// receiver sends NMEA even without a fix. "$G" is looked for rather than
// any byte because a floating RX pin produces noise now and then, but not
// the start of a sentence; every GNSS talker starts with G (GP, GN, GL...).
// At the wrong baud rate the same bytes come out as garbage, never as "$G".
bool listenForNmea(uint32_t timeoutMs) {
  uint32_t start = millis();
  bool dollar = false;
  while (millis() - start < timeoutMs) {
    while (gpsSerial.available() > 0) {
      char c = gpsSerial.read();
      if (dollar && c == 'G') return true;
      dollar = (c == '$');
    }
    delay(1);
  }
  return false;
}

void sendNmea(const char *body) {
  char sentence[80];
  size_t n = core::nmeaSentence(body, sentence, sizeof(sentence));
  if (n) gpsSerial.write((const uint8_t *)sentence, n);
  delay(30);   // let the module digest it before the next one
}

void sendUbx(uint8_t msgClass, uint8_t msgId, const uint8_t *payload, uint16_t length) {
  uint8_t frame[64];
  size_t n = core::ubxFrame(msgClass, msgId, payload, length, frame, sizeof(frame));
  if (n) gpsSerial.write(frame, n);
}

// The next complete UBX message, whatever it is. A small state machine
// over the incoming bytes, which carry NMEA sentences meanwhile: sync,
// class, id, length, payload, checksum. Returns the payload length (bytes
// beyond `size` are counted but not stored), -1 on timeout.
static int nextUbx(uint8_t &msgClass, uint8_t &msgId, uint8_t *payload, uint16_t size,
                   uint32_t timeoutMs) {
  uint32_t start = millis();
  uint8_t header[4];
  int state = 0;
  uint16_t length = 0, got = 0;
  uint8_t ckA = 0, ckB = 0;

  while (millis() - start < timeoutMs) {
    if (!gpsSerial.available()) { delay(1); continue; }
    uint8_t b = gpsSerial.read();
    switch (state) {
      case 0: state = (b == 0xB5) ? 1 : 0; break;
      case 1: state = (b == 0x62) ? 2 : 0; break;
      case 2: case 3: case 4: case 5:
        header[state - 2] = b;
        if (state == 5) {
          length = header[2] | (header[3] << 8);
          got = 0;
          ckA = ckB = 0;
          for (uint8_t h : header) { ckA += h; ckB += ckA; }
          state = length ? 6 : 7;
        } else {
          state++;
        }
        break;
      case 6:
        if (got < size) payload[got] = b;
        ckA += b; ckB += ckA;
        if (++got == length) state = 7;
        break;
      case 7: state = (b == ckA) ? 8 : 0; break;
      case 8:
        if (b == ckB) {
          msgClass = header[0];
          msgId = header[1];
          return length;
        }
        state = 0;
        break;
    }
  }
  return -1;
}

int waitUbx(uint8_t msgClass, uint8_t msgId, uint8_t *payload, uint16_t size, uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    uint8_t cls, id;
    int n = nextUbx(cls, id, payload, size, timeoutMs - (millis() - start));
    if (n < 0) return -1;
    if (cls == msgClass && id == msgId) return n;
  }
  return -1;
}

// ACK-ACK is class 0x05 id 0x01, ACK-NAK 0x05 0x00; both carry the class
// and id of the command they answer.
int waitAck(uint8_t msgClass, uint8_t msgId, uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    uint8_t cls, id, p[2];
    int n = nextUbx(cls, id, p, 2, timeoutMs - (millis() - start));
    if (n < 0) return -1;
    if (cls == 0x05 && n == 2 && p[0] == msgClass && p[1] == msgId) return id == 0x01 ? 1 : 0;
  }
  return -1;
}

bool followBaud(uint32_t baud, uint32_t previous) {
  delay(100);   // the module finishes sending at the old speed
  gpsSerial.updateBaudRate(baud);
  drain();
  if (listenForNmea(1500)) return true;
  gpsSerial.updateBaudRate(previous);
  drain();
  return false;
}

// ===========================================================================
// Boot
// ===========================================================================

// Counts how many RMC sentences arrive in a while: reports per second, the
// one figure that says whether the configuration worked.
static uint8_t measureRate() {
  const uint32_t WINDOW_MS = 1500;
  const char *pattern = "RMC,";
  uint8_t matched = 0, count = 0;
  bool inHeader = false;
  uint32_t start = millis();
  drain();
  while (millis() - start < WINDOW_MS) {
    if (!gpsSerial.available()) { delay(1); continue; }
    char c = gpsSerial.read();
    if (c == '$') { inHeader = true; matched = 0; continue; }
    if (!inHeader) continue;
    // "$GNRMC," : skip the two talker letters, then match "RMC,"
    if (matched < 4 && c == pattern[matched]) {
      if (++matched == 4) { count++; inHeader = false; }
    } else if (c == ',') {
      inHeader = false;
    } else {
      matched = (c == pattern[0]) ? 1 : 0;
    }
  }
  float perSecond = count * 1000.0f / WINDOW_MS;
  if (perSecond >= 7.5f) return 10;
  if (perSecond >= 3.5f) return 5;
  if (perSecond >= 1.5f) return 2;
  return 1;
}

bool begin() {
  // The usual factory speeds, most common first. A module answering at
  // none of them is either missing or wired TX/RX the wrong way round.
  const uint32_t BAUDS[] = {9600, 38400, 115200, 57600};
  const uint32_t LISTEN_MS = 1100;   // every module sends at least once a second

  uint32_t baud = 0;
  for (uint32_t candidate : BAUDS) {
    open(candidate);
    if (listenForNmea(LISTEN_MS)) { baud = candidate; break; }
  }
  if (baud == 0) {
    open(BAUDS[0]);   // stay listening: a slow module may still wake up
    return false;
  }

  const char *ublox = ubloxConfigure(baud);
  if (ublox != nullptr) {
    foundName = ublox;
  } else {
    otherFamiliesConfigure(baud);
    foundName = "NMEA";
  }

  rateHz = measureRate();
  Serial.printf("  GPS: %s at %lu baud, %u reports/s\n", foundName, (unsigned long)baud,
                (unsigned)rateHz);
  return true;
}

const char *name() { return foundName; }

uint8_t measuredRateHz() { return rateHz; }

uint32_t fixTimeoutMs() {
  uint32_t interval = 1000 / rateHz;
  uint32_t timeout = interval * 7 / 2;
  return timeout < 500 ? 500 : timeout;
}

}  // namespace gpsModule
