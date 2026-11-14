/*
  All the weather sensor drivers, one per #if block. The sensors fall into
  two families by how they are read, which explains the layout:
    A) two functions returning float, NaN on failure
       (DHT22, SHT31, Si7021, HTU21D, BME280)
    B) Adafruit "unified sensor" API: one call fills two event structs
       (SHT4x, HTU31D, AHT20)
  Naming the object "sensor" in every block lets read() be written once per
  family instead of once per chip.
*/

#include "WeatherSensor.h"

#include <Wire.h>

#include "../../Config.h"

// ---------------------------------------------------------------------------
// Sensor object + boot name: everything that differs between chips.
// ---------------------------------------------------------------------------

#if defined(WEATHER_DHT22)
  #include <DHT.h>
  static DHT sensor(PIN_DHT, DHT22);
  #define WEATHER_NAME "DHT22"

#elif defined(WEATHER_SHT4X)
  #include <Adafruit_SHT4x.h>
  static Adafruit_SHT4x sensor;
  #define WEATHER_NAME "SHT4x"
  #define WEATHER_EVENT_API

#elif defined(WEATHER_SHT31)
  #include <Adafruit_SHT31.h>
  static Adafruit_SHT31 sensor;
  #define WEATHER_NAME "SHT31"

#elif defined(WEATHER_HTU31D)
  #include <Adafruit_HTU31D.h>
  static Adafruit_HTU31D sensor;
  #define WEATHER_NAME "HTU31D"
  #define WEATHER_EVENT_API

#elif defined(WEATHER_SI7021)
  #include <Adafruit_Si7021.h>
  static Adafruit_Si7021 sensor;
  #define WEATHER_NAME "Si7021"

#elif defined(WEATHER_HTU21D)
  #include <Adafruit_HTU21DF.h>
  static Adafruit_HTU21DF sensor;
  #define WEATHER_NAME "HTU21D"

#elif defined(WEATHER_AHT20)
  #include <Adafruit_AHTX0.h>
  static Adafruit_AHTX0 sensor;
  #define WEATHER_NAME "AHT20"
  #define WEATHER_EVENT_API

#elif defined(WEATHER_BME280)
  #include <Adafruit_BME280.h>
  static Adafruit_BME280 sensor;
  #define WEATHER_NAME "BME280"
#endif

namespace weatherSensor {

bool begin() {
#if defined(WEATHER_DHT22)
  // The DHT22 has no identity register: begin() only configures the pin, so
  // the only way to know it is there is to read it (an absent sensor gives
  // NaN). Several attempts are needed: the chip takes about 2 s after
  // power-up before its first good value.
  //
  // The second argument is the library's "force": without it close reads
  // return the cached value and every retry would be a copy of the first.
  //
  // A present sensor exits on the first pass; an absent one costs about 1 s.
  const uint8_t  ATTEMPTS         = 4;
  const uint16_t ATTEMPT_PAUSE_MS = 350;

  sensor.begin();
  for (uint8_t attempt = 0; attempt < ATTEMPTS; attempt++) {
    if (attempt > 0) delay(ATTEMPT_PAUSE_MS);
    if (!isnan(sensor.readTemperature(false, true))) return true;
  }
  return false;

#elif defined(WEATHER_SHT4X)
  if (!sensor.begin()) return false;
  sensor.setPrecision(SHT4X_HIGH_PRECISION);
  // Heater OFF: it clears condensation but warms the chip by several
  // degrees - and air temperature is half of the grip risk formula.
  sensor.setHeater(SHT4X_NO_HEATER);
  return true;

#elif defined(WEATHER_SHT31)
  return sensor.begin(I2C_ADDR_SHT31);

#elif defined(WEATHER_HTU31D)
  return sensor.begin(I2C_ADDR_HTU31D);

#elif defined(WEATHER_SI7021) || defined(WEATHER_HTU21D)
  return sensor.begin();

#elif defined(WEATHER_AHT20)
  return sensor.begin(&Wire, 0, I2C_ADDR_AHT20);

#elif defined(WEATHER_BME280)
  if (sensor.begin(I2C_ADDR_BME280, &Wire)) return true;
  // The only driver that explains its own failure, for a precise reason:
  // the BMP280 looks the same and uses the same address as the BME280 but
  // has no humidity sensor, and without humidity there is no grip risk.
  // Only the chip ID tells them apart.
  Serial.printf("  BME280: chip ID 0x%X", (unsigned)sensor.sensorID());
  Serial.println(sensor.sensorID() == 0x58
      ? F(" = this is a BMP280! No humidity, so no grip risk.")
      : F(" = no answer, or not a BME280. Try address 0x77."));
  return false;
#endif
}

bool read(float &temperature, float &humidity) {
#ifdef WEATHER_EVENT_API
  // Family B. Mind the order: humidity first.
  sensors_event_t humidityEvent, temperatureEvent;
  if (!sensor.getEvent(&humidityEvent, &temperatureEvent)) return false;
  temperature = temperatureEvent.temperature;
  humidity    = humidityEvent.relative_humidity;
  return true;
#else
  // Family A. The two quantities are validated separately.
  float t = sensor.readTemperature();
  float h = sensor.readHumidity();
  bool valid = false;
  if (!isnan(t)) { temperature = t; valid = true; }
  if (!isnan(h)) { humidity    = h; valid = true; }
  return valid;
#endif
}

const char *name() { return WEATHER_NAME; }

}  // namespace weatherSensor
