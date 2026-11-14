/*
  WheelStat - Config.h
  ===========================================================================
  What changes from one build to the next: the interface language, which
  components are fitted, how they are wired and how the IMU is mounted.

  Does it change if I fit different parts, or the same parts mounted
  differently?  yes -> here   no -> src/Tuning.h

  On a new build check the pinout and, above all, the IMU ORIENTATION: it is
  the one setting that cannot be worked out on paper.

  Only macros and plain constants, no library includes: every module in src/
  includes this file. The driver mechanism is explained in docs/DRIVER.md.
  ===========================================================================
*/

#ifndef WHEELSTAT_CONFIG_H
#define WHEELSTAT_CONFIG_H

#include <Arduino.h>

// ===========================================================================
// LANGUAGE
// ===========================================================================
// Of the OLED and of the website. Serial output and exported files are
// always English.
//
// The #if !defined(...) guards, here and below, let a macro passed on the
// command line with -D win instead of enabling two choices at once.

#if !defined(LANG_IT) && !defined(LANG_EN)
  #define LANG_IT     // Italiano
// #define LANG_EN    // English
#endif

// ===========================================================================
// DRIVER SELECTION
// ===========================================================================
// One macro per category. Compatibility notes: docs/HARDWARE.md.

#if !defined(WEATHER_DHT22)  && !defined(WEATHER_SHT4X)  && !defined(WEATHER_SHT31) && \
    !defined(WEATHER_HTU31D) && !defined(WEATHER_SI7021) && !defined(WEATHER_HTU21D) && \
    !defined(WEATHER_AHT20)  && !defined(WEATHER_BME280)
  #define WEATHER_DHT22     // fitted
// #define WEATHER_SHT4X    // SHT40/41/45, the most accurate: the one to buy today
// #define WEATHER_SHT31    // previous generation of the SHT4x
// #define WEATHER_HTU31D   // good price/accuracy trade-off
// #define WEATHER_SI7021   // reliable classic
// #define WEATHER_HTU21D   // predecessor of the HTU31D
// #define WEATHER_AHT20    // the cheapest, modest accuracy
// #define WEATHER_BME280   // also measures pressure, which the firmware ignores
#endif

#if !defined(DISPLAY_SSD1306) && !defined(DISPLAY_SSD1309) && \
    !defined(DISPLAY_SH1106)  && !defined(DISPLAY_SH1107)
  #define DISPLAY_SSD1306  // fitted
// #define DISPLAY_SSD1309 // different controller, same library as the SSD1306
// #define DISPLAY_SH1106  // 128x64, identical layout
// #define DISPLAY_SH1107  // CAREFUL: almost always 128x128, which is not supported
#endif

// A single implementation today: the macro is the hook for the successor of
// the BNO055, which is end-of-life.
#if !defined(IMU_BNO055)
  #define IMU_BNO055
#endif

// The two u-blox generations do not cover each other: the M10 dropped the
// old configuration messages. The wrong driver leaves the module at 1 Hz
// while GPS_FIX_TIMEOUT_MS is tuned for the high rate, and the fix flickers.
#if !defined(GPS_UBLOX) && !defined(GPS_UBLOX_M10) && !defined(GPS_NMEA_GENERIC)
  #define GPS_UBLOX          // fitted: NEO-6M/8M, Beitian BN-220/BN-880
// #define GPS_UBLOX_M10     // MAX-M10S, MIA-M10, Beitian M10 series
// #define GPS_NMEA_GENERIC  // any non-u-blox module: stays at 1 Hz
#endif

// ===========================================================================
// PINS
// ===========================================================================
// One pinout per chip family, picked from the board selected in the IDE.
// For a board that is not listed, copy the closest block.
//
// If you move a pin:
//   - never a BUTTON on a strapping pin: read as pressed at power-up, the
//     board would not boot. I2C may sit on one, its pull-ups hold it high;
//   - nothing on the pins of the internal SPI flash or of the native USB.
//
// Buttons go from the pin to GND (internal pull-up). PIN_GPS_RX is where the
// ESP32 receives, so the module's TX goes there.

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // Strapping: 0, 3, 45, 46. USB: 19, 20.
  constexpr uint8_t PIN_BTN_UP   = 5;
  constexpr uint8_t PIN_BTN_DOWN = 6;
  constexpr uint8_t PIN_BTN_OK   = 7;
  constexpr uint8_t PIN_BTN_LOG  = 15;
  constexpr uint8_t PIN_DHT      = 4;
  constexpr uint8_t PIN_I2C_SDA  = 8;
  constexpr uint8_t PIN_I2C_SCL  = 9;
  constexpr uint8_t PIN_GPS_RX   = 18;
  constexpr uint8_t PIN_GPS_TX   = 17;
  constexpr uint8_t GPS_UART_NUM = 2;

#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  // Only GPIO 0-21. Strapping: 2, 8, 9. Flash: 12-17. USB: 18, 19.
  constexpr uint8_t PIN_BTN_UP   = 3;
  constexpr uint8_t PIN_BTN_DOWN = 4;
  constexpr uint8_t PIN_BTN_OK   = 5;
  constexpr uint8_t PIN_BTN_LOG  = 6;
  constexpr uint8_t PIN_DHT      = 10;
  constexpr uint8_t PIN_I2C_SDA  = 8;
  constexpr uint8_t PIN_I2C_SCL  = 9;
  constexpr uint8_t PIN_GPS_RX   = 0;
  constexpr uint8_t PIN_GPS_TX   = 1;
  // Only two UARTs, and 0 is the serial monitor. A 2 here would compile and
  // give a port that never receives.
  constexpr uint8_t GPS_UART_NUM = 1;

#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  // Strapping: 0, 45, 46. USB: 19, 20. Flash: 26-32.
  constexpr uint8_t PIN_BTN_UP   = 5;
  constexpr uint8_t PIN_BTN_DOWN = 6;
  constexpr uint8_t PIN_BTN_OK   = 7;
  constexpr uint8_t PIN_BTN_LOG  = 15;
  constexpr uint8_t PIN_DHT      = 4;
  constexpr uint8_t PIN_I2C_SDA  = 8;
  constexpr uint8_t PIN_I2C_SCL  = 9;
  constexpr uint8_t PIN_GPS_RX   = 18;
  constexpr uint8_t PIN_GPS_TX   = 17;
  constexpr uint8_t GPS_UART_NUM = 1;   // only two UARTs, like the C3

#elif defined(CONFIG_IDF_TARGET_ESP32)
  // Classic ESP32 (DevKit V1): the fitted board, the only one tested on
  // hardware. Strapping: 0, 2, 12, 15. GPIO 34-39 are input only with no
  // pull-up: no good for the buttons.
  constexpr uint8_t PIN_BTN_UP   = 13;
  constexpr uint8_t PIN_BTN_DOWN = 25;
  constexpr uint8_t PIN_BTN_OK   = 14;
  constexpr uint8_t PIN_BTN_LOG  = 27;
  constexpr uint8_t PIN_DHT      = 4;
  constexpr uint8_t PIN_I2C_SDA  = 21;
  constexpr uint8_t PIN_I2C_SCL  = 22;
  constexpr uint8_t PIN_GPS_RX   = 16;
  constexpr uint8_t PIN_GPS_TX   = 17;
  constexpr uint8_t GPS_UART_NUM = 2;

#else
  // No silent fallback to the classic pinout: on an unlisted chip it would
  // boot and read sensors that were never connected.
  #error "Unsupported board. Use a classic ESP32, S2, S3 or C3, or add its pin block in Config.h by copying the closest one. See docs/HARDWARE.md."
#endif

// ===========================================================================
// I2C BUS
// ===========================================================================
// OLED, IMU and an I2C weather sensor share the bus: before buying a part,
// check its address is free.
constexpr uint8_t I2C_ADDR_OLED   = 0x3C;
constexpr uint8_t I2C_ADDR_BNO055 = 0x28;  // 0x29 on some breakouts (ADR jumper)

// Only the weather sensors whose library takes an address.
constexpr uint8_t I2C_ADDR_SHT31  = 0x44;  // 0x45 with ADDR tied to VDD
constexpr uint8_t I2C_ADDR_HTU31D = 0x40;  // 0x41 with ADDR tied to VDD
constexpr uint8_t I2C_ADDR_AHT20  = 0x38;
constexpr uint8_t I2C_ADDR_BME280 = 0x76;  // 0x77 with SDO tied to VDD

// 400 kHz: an OLED frame takes ~23 ms instead of ~90. Slower, 10 FPS do not
// fit.
constexpr uint32_t I2C_CLOCK_HZ = 400000;

// ===========================================================================
// WEATHER SENSOR TIMING
// ===========================================================================
// A physical limit of the part, which is why it is here and not in Tuning.h.

#if defined(WEATHER_DHT22)
  // Asked more often than every 2 s, the DHT22 repeats its last reading.
  constexpr uint32_t WEATHER_INTERVAL_MS = 2000;
#else
  // Faster would only load the bus shared with OLED and IMU.
  constexpr uint32_t WEATHER_INTERVAL_MS = 1000;
#endif

// ===========================================================================
// DISPLAY
// ===========================================================================
// The pages are hand-drawn for 128x64: changing these does NOT re-flow them.
constexpr uint8_t SCREEN_WIDTH  = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;

// One character of the built-in font at size 1.
constexpr uint8_t FONT_CHAR_WIDTH = 6;

// ===========================================================================
// IMU ORIENTATION - the first thing to set on a new build
// ===========================================================================
// Get it wrong and nothing fails: the numbers are just believable and false.
// The procedure, with the four checks to do box in hand, is in the README
// ("IMU orientation").

// STEP 1: how the breakout sits. "Up" is the component side facing the sky.
#if !defined(IMU_MOUNT_UPRIGHT) && !defined(IMU_MOUNT_UPSIDE_DOWN) && \
    !defined(IMU_MOUNT_MANUAL)
  #define IMU_MOUNT_UPSIDE_DOWN   // fitted: board flipped over
// #define IMU_MOUNT_UPRIGHT      // components up, board flat
// #define IMU_MOUNT_MANUAL       // neither: pick the preset below
#endif

// The BNO055 remaps its axes inside the chip, with eight presets P0-P7.
#if defined(IMU_MOUNT_UPSIDE_DOWN)
  #define IMU_BNO055_REMAP P5
#elif defined(IMU_MOUNT_UPRIGHT)
  #define IMU_BNO055_REMAP P1   // the chip's factory orientation
#elif defined(IMU_MOUNT_MANUAL)
  #define IMU_BNO055_REMAP P1   // <-- change here: P0 ... P7
#endif

// STEP 2: flip the ones that come out reversed. A sign reverses a direction;
// two SWAPPED quantities (lean moves when the nose lifts) are a wrong step 1.
constexpr float SIGN_LEAN   =  1.0f;   // lean right must read positive
constexpr float SIGN_PITCH  = -1.0f;   // nose up must read as a wheelie
constexpr float SIGN_G_LONG =  1.0f;   // accelerating must read positive
constexpr float SIGN_G_LAT  = -1.0f;   // pushed left, the G dot must go left

// ===========================================================================
// GPS MODULE
// ===========================================================================

constexpr uint32_t GPS_BAUD = 9600;  // NEO-6M/8M factory default

#if defined(GPS_UBLOX) || defined(GPS_UBLOX_M10)
  // 5 Hz fits in 9600 baud only because the driver turns off the sentences
  // nobody reads; 10 Hz would need a higher baud rate.
  constexpr uint8_t GPS_RATE_HZ = 5;

  // 3.5 missed reports before the fix is called stale. Goes with GPS_RATE_HZ.
  constexpr uint32_t GPS_FIX_TIMEOUT_MS = 700;

#elif defined(GPS_NMEA_GENERIC)
  // The rate cannot be set without UBX, and nearly every module ships at
  // 1 Hz: with 700 ms the fix would go stale between one report and the next.
  constexpr uint32_t GPS_FIX_TIMEOUT_MS = 3000;
#endif

// ===========================================================================
// ACCESS POINT
// ===========================================================================
// Network name until it is changed from the website (12 characters at most).
// No password here on purpose: each device makes up its own the first time
// the WiFi is switched on and shows it on the OLED.
constexpr char WIFI_DEFAULT_NAME[] = "WheelStat";

// ===========================================================================
// SANITY CHECKS
// ===========================================================================

#if defined(LANG_IT) && defined(LANG_EN)
  #error "Config.h: select only one language (LANG_IT or LANG_EN)"
#endif

#if !defined(WEATHER_DHT22)  && !defined(WEATHER_SHT4X)  && !defined(WEATHER_SHT31) && \
    !defined(WEATHER_HTU31D) && !defined(WEATHER_SI7021) && !defined(WEATHER_HTU21D) && \
    !defined(WEATHER_AHT20)  && !defined(WEATHER_BME280)
  #error "Config.h: no weather driver selected (see DRIVER SELECTION)"
#endif

#if !defined(DISPLAY_SSD1306) && !defined(DISPLAY_SSD1309) && \
    !defined(DISPLAY_SH1106)  && !defined(DISPLAY_SH1107)
  #error "Config.h: no display driver selected (see DRIVER SELECTION)"
#endif

#if !defined(GPS_UBLOX) && !defined(GPS_UBLOX_M10) && !defined(GPS_NMEA_GENERIC)
  #error "Config.h: no GPS driver selected (see DRIVER SELECTION)"
#endif

#endif  // WHEELSTAT_CONFIG_H
