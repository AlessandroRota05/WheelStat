#pragma once

// WheelStat - src/drivers/gps/GpsLink.h
// Internal to the GPS driver: the serial-line helpers shared by the family
// files, and what each family file provides.

#include <Arduino.h>

namespace gpsModule {

// --- The line -------------------------------------------------------------------
void open(uint32_t baud);
void drain();                                       // throw away what is waiting
bool listenForNmea(uint32_t timeoutMs);             // true at the first "$G"
void sendNmea(const char *body);                    // adds $, checksum, CR LF
void sendUbx(uint8_t msgClass, uint8_t msgId, const uint8_t *payload, uint16_t length);

// Waits for a UBX message of this class/id; copies up to `size` payload
// bytes into `payload` and returns its length, -1 on timeout.
int waitUbx(uint8_t msgClass, uint8_t msgId, uint8_t *payload, uint16_t size, uint32_t timeoutMs);

// UBX-ACK for a command: 1 = ACK, 0 = NAK, -1 = no answer.
int waitAck(uint8_t msgClass, uint8_t msgId, uint32_t timeoutMs);

// Switches the ESP32 side to `baud` and checks NMEA still arrives; if not,
// goes back to `previous` and returns false.
bool followBaud(uint32_t baud, uint32_t previous);

// --- The families ----------------------------------------------------------------
// u-blox: asks the version; if a u-blox answers, configures it (rate in Hz
// asked, faster baud) and returns its name. nullptr if no u-blox answered.
const char *ubloxConfigure(uint32_t &baud);

// CASIC and Airoha commands, sent blindly: each family ignores the other's.
void otherFamiliesConfigure(uint32_t &baud);

}  // namespace gpsModule
