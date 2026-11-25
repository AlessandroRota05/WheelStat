#pragma once

/*
  WheelStat - src/drivers/I2cBus.h
  ===========================================================================
  The few I2C transactions the drivers written without a library need. One
  place for them keeps every driver file to what is specific to its chip:
  addresses, commands, timings.

  Every function returns false on a NACK or a short read, never blocks for
  long, and works on the bus Wire was started on in App.
  ===========================================================================
*/

#include <Arduino.h>

namespace i2c {

// True if a device acknowledges this address.
bool present(uint8_t address);

// Raw write and read.
bool write(uint8_t address, const uint8_t *data, size_t length);
bool read(uint8_t address, uint8_t *out, size_t length);

// A 16-bit command, most significant byte first (Sensirion style).
bool command16(uint8_t address, uint16_t command);

// One register byte.
bool writeRegister(uint8_t address, uint8_t reg, uint8_t value);

// Pointer to `reg`, then `length` bytes back with a repeated start.
bool readRegisters(uint8_t address, uint8_t reg, uint8_t *out, size_t length);

// Convenience: one register byte, or `fallback` if the chip did not answer.
uint8_t readRegister(uint8_t address, uint8_t reg, uint8_t fallback = 0xFF);

}  // namespace i2c
