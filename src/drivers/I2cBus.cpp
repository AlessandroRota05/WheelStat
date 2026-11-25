#include "I2cBus.h"

#include <Wire.h>

namespace i2c {

bool present(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool write(uint8_t address, const uint8_t *data, size_t length) {
  Wire.beginTransmission(address);
  Wire.write(data, length);
  return Wire.endTransmission() == 0;
}

bool read(uint8_t address, uint8_t *out, size_t length) {
  if (Wire.requestFrom(address, (uint8_t)length) != length) return false;
  for (size_t i = 0; i < length; i++) out[i] = Wire.read();
  return true;
}

bool command16(uint8_t address, uint16_t command) {
  const uint8_t bytes[2] = {(uint8_t)(command >> 8), (uint8_t)(command & 0xFF)};
  return write(address, bytes, 2);
}

bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
  const uint8_t bytes[2] = {reg, value};
  return write(address, bytes, 2);
}

bool readRegisters(uint8_t address, uint8_t reg, uint8_t *out, size_t length) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;  // repeated start
  return read(address, out, length);
}

uint8_t readRegister(uint8_t address, uint8_t reg, uint8_t fallback) {
  uint8_t value;
  return readRegisters(address, reg, &value, 1) ? value : fallback;
}

}  // namespace i2c
