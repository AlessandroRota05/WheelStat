#include "SensorMath.h"

namespace core {

uint8_t crc8(const uint8_t *data, size_t length) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}

uint16_t crc16Modbus(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
  }
  return crc;
}

static float clampHumidity(float h) { return h < 0 ? 0 : (h > 100 ? 100 : h); }

float sensirionTemperature(uint16_t raw) { return -45.0f + 175.0f * raw / 65535.0f; }
float sht3xHumidity(uint16_t raw)        { return clampHumidity(100.0f * raw / 65535.0f); }
float sht4xHumidity(uint16_t raw)        { return clampHumidity(-6.0f + 125.0f * raw / 65535.0f); }
float si7021Temperature(uint16_t raw)    { return 175.72f * raw / 65536.0f - 46.85f; }
float si7021Humidity(uint16_t raw)       { return clampHumidity(125.0f * raw / 65536.0f - 6.0f); }
float hdc1080Temperature(uint16_t raw)   { return 165.0f * raw / 65536.0f - 40.0f; }
float hdc1080Humidity(uint16_t raw)      { return clampHumidity(100.0f * raw / 65536.0f); }
float htu31Temperature(uint16_t raw)     { return -40.0f + 165.0f * raw / 65535.0f; }
float htu31Humidity(uint16_t raw)        { return clampHumidity(100.0f * raw / 65535.0f); }

bool ahtDecode(const uint8_t f[6], float &temperature, float &humidity) {
  if (f[0] & 0x80) return false;  // busy bit: conversion not finished
  uint32_t rawH = ((uint32_t)f[1] << 12) | ((uint32_t)f[2] << 4) | (f[3] >> 4);
  uint32_t rawT = ((uint32_t)(f[3] & 0x0F) << 16) | ((uint32_t)f[4] << 8) | f[5];
  humidity    = clampHumidity(rawH * 100.0f / 1048576.0f);
  temperature = rawT * 200.0f / 1048576.0f - 50.0f;
  return true;
}

bool am2320Decode(const uint8_t f[8], float &temperature, float &humidity) {
  if (f[0] != 0x03 || f[1] != 0x04) return false;
  uint16_t crc = (uint16_t)(f[7] << 8 | f[6]);   // sent low byte first
  if (crc16Modbus(f, 6) != crc) return false;
  humidity = be16(f + 2) / 10.0f;
  uint16_t t = be16(f + 4);
  temperature = (t & 0x8000) ? -(t & 0x7FFF) / 10.0f : t / 10.0f;   // sign and magnitude
  return true;
}

Bme280Calibration bme280Calibration(const uint8_t a[26], const uint8_t b[7]) {
  Bme280Calibration c;
  c.T1 = (uint16_t)(a[1] << 8 | a[0]);
  c.T2 = (int16_t)(a[3] << 8 | a[2]);
  c.T3 = (int16_t)(a[5] << 8 | a[4]);
  c.H1 = a[25];
  c.H2 = (int16_t)(b[1] << 8 | b[0]);
  c.H3 = b[2];
  c.H4 = (int16_t)(((int8_t)b[3] << 4) | (b[4] & 0x0F));
  c.H5 = (int16_t)(((int8_t)b[5] << 4) | (b[4] >> 4));
  c.H6 = (int8_t)b[6];
  return c;
}

float bme280Temperature(const Bme280Calibration &c, int32_t adcT, int32_t &tFine) {
  int32_t var1 = ((((adcT >> 3) - ((int32_t)c.T1 << 1))) * ((int32_t)c.T2)) >> 11;
  int32_t var2 = (((((adcT >> 4) - ((int32_t)c.T1)) * ((adcT >> 4) - ((int32_t)c.T1))) >> 12) *
                  ((int32_t)c.T3)) >> 14;
  tFine = var1 + var2;
  return ((tFine * 5 + 128) >> 8) / 100.0f;
}

float bme280Humidity(const Bme280Calibration &c, int32_t adcH, int32_t tFine) {
  int32_t v = tFine - 76800;
  v = (((((adcH << 14) - (((int32_t)c.H4) << 20) - (((int32_t)c.H5) * v)) + 16384) >> 15) *
       (((((((v * ((int32_t)c.H6)) >> 10) * (((v * ((int32_t)c.H3)) >> 11) + 32768)) >> 10) +
          2097152) * ((int32_t)c.H2) + 8192) >> 14));
  v = v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)c.H1)) >> 4);
  v = v < 0 ? 0 : v;
  v = v > 419430400 ? 419430400 : v;
  return (uint32_t)(v >> 12) / 1024.0f;
}

}  // namespace core
