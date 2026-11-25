#pragma once

// WheelStat - src/drivers/weather/WeatherDrivers.h
// Internal to the weather sensor drivers: what each chip family file
// provides, and the list WeatherSensor.cpp walks at boot.
//
// Adding a sensor: a new file here defining one Driver, and one line in
// the list in WeatherSensor.cpp. Nothing else changes.

#include <Arduino.h>

namespace weatherSensor {

struct Driver {
  // Looks for the chip and prepares it. Returns its name, nullptr if absent.
  const char *(*detect)();
  bool (*read)(float &temperature, float &humidity);
  uint32_t intervalMs;
};

extern const Driver SENSIRION;   // SHT4x, SHT3x, SHTC3     0x44 0x45 0x70
extern const Driver AHT;         // AHT10, AHT20, AHT21/25  0x38
extern const Driver HDC1080;     //                         0x40
extern const Driver SI70XX;      // Si7021, HTU21D, SHT21   0x40
extern const Driver HTU31;       // HTU31D                  0x40 0x41
extern const Driver AM2320;      //                         0x5C
extern const Driver BME280;      // BME280 (not BMP280)     0x76 0x77
extern const Driver DHT_SENSOR;  // DHT22/AM2302, DHT11     PIN_DHT

}  // namespace weatherSensor
