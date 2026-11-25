#pragma once

/*
  WheelStat - src/core/SensorMath.h
  ===========================================================================
  The arithmetic of the sensors the firmware reads by itself, without a
  library: checksums and the conversion from raw counts to units, each from
  its datasheet. Kept here, away from the I2C code, so it can be checked on
  a PC against the datasheets' own examples.
  ===========================================================================
*/

#include <stddef.h>
#include <stdint.h>

namespace core {

// CRC-8, polynomial 0x31, start 0xFF: Sensirion (SHT3x/4x/C3), AHT20/21,
// HTU31D all use it on every 16-bit word they send.
uint8_t crc8(const uint8_t *data, size_t length);

// CRC-16/MODBUS: the AM2320's frame check.
uint16_t crc16Modbus(const uint8_t *data, size_t length);

inline uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

// --- Temperature / humidity, raw 16-bit words --------------------------------
float sensirionTemperature(uint16_t raw);   // SHT3x, SHT4x, SHTC3
float sht3xHumidity(uint16_t raw);          // also SHTC3
float sht4xHumidity(uint16_t raw);          // clamped to 0-100
float si7021Temperature(uint16_t raw);      // also HTU21D
float si7021Humidity(uint16_t raw);
float hdc1080Temperature(uint16_t raw);
float hdc1080Humidity(uint16_t raw);
float htu31Temperature(uint16_t raw);
float htu31Humidity(uint16_t raw);

// AHT10/20/21/25: one 6-byte frame (status, then 20 bits of humidity and
// 20 of temperature packed together). False while the chip is still busy.
bool ahtDecode(const uint8_t frame[6], float &temperature, float &humidity);

// AM2320: 8-byte answer to "read 4 registers from 0" (function code,
// count, humidity, temperature, CRC). False if malformed.
bool am2320Decode(const uint8_t frame[8], float &temperature, float &humidity);

// --- BME280 --------------------------------------------------------------------
// The calibration words stored in the chip, and Bosch's compensation
// formulas (datasheet section 4.2.3, fixed-point version).
struct Bme280Calibration {
  uint16_t T1; int16_t T2, T3;
  uint8_t  H1; int16_t H2; uint8_t H3; int16_t H4, H5; int8_t H6;
};

// Calibration from the chip's registers 0x88..0xA1 (26 bytes) and
// 0xE1..0xE7 (7 bytes).
Bme280Calibration bme280Calibration(const uint8_t block1[26], const uint8_t block2[7]);

// tFine is computed by the temperature and needed by the humidity.
float bme280Temperature(const Bme280Calibration &c, int32_t adcT, int32_t &tFine);
float bme280Humidity(const Bme280Calibration &c, int32_t adcH, int32_t tFine);

}  // namespace core
