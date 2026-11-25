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
// COMPONENTS
// ===========================================================================
// Most of the hardware is recognised at boot and needs nothing here:
//   weather sensor  DHT22/DHT11, SHT4x/SHT3x/SHTC3, AHT10/20/21, HDC1080,
//                   Si7021/HTU21D, HTU31D, AM2320, BME280
//   IMU             BNO055, MPU6050 family, ICM-20948, LSM6DS family
//                   (+ BNO085/086 if enabled below)
//   GPS             speed, family (u-blox, CASIC, Airoha, generic NMEA), rate
//   display         its I2C address
//   microSD         whether a card is in
// What the firmware found is on the boot screen and the serial monitor.
// Compatibility notes: docs/HARDWARE.md.

// The display controller is the one thing that cannot be asked: panels with
// different controllers answer on the same address. 1.3" modules are
// usually SH1106, 0.96" ones SSD1306.
#if !defined(DISPLAY_SSD1306) && !defined(DISPLAY_SSD1309) && \
    !defined(DISPLAY_SH1106)  && !defined(DISPLAY_SH1107)
  #define DISPLAY_SSD1306  // fitted
// #define DISPLAY_SSD1309 // different controller, same library as the SSD1306
// #define DISPLAY_SH1106  // 128x64, identical layout
// #define DISPLAY_SH1107  // CAREFUL: almost always 128x128, which is not supported
#endif

// A display wired to SPI instead of I2C (pins below). Faster, but SPI
// cannot tell whether a panel is there: it is assumed it is.
// #define DISPLAY_SPI

// The IMUs that need a library, which costs flash: each one enabled here
// is looked for at boot. The others are always there (no library).
#define IMU_WITH_BNO055      // Adafruit BNO055 - fitted
// #define IMU_WITH_BNO08X   // Adafruit BNO08x - BNO085/086, about 10 KB of flash

// Sessions on a microSD when a card is in (FAT32, any size). The card shares
// the SPI bus with an SPI display. Not on the C3: it has no pins left.
#if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(NO_SD_CARD)
  #define SD_CARD
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
  // SPI bus, shared by the microSD and an SPI display (DISPLAY_SPI).
  constexpr uint8_t PIN_SPI_SCK  = 12;
  constexpr uint8_t PIN_SPI_MISO = 13;
  constexpr uint8_t PIN_SPI_MOSI = 11;
  constexpr uint8_t PIN_SD_CS    = 10;
  constexpr int8_t  PIN_OLED_CS  = 14;
  constexpr int8_t  PIN_OLED_DC  = 16;
  constexpr int8_t  PIN_OLED_RST = 21;   // -1: tied to the board's reset

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
  // SPI bus, shared by the microSD and an SPI display (DISPLAY_SPI).
  constexpr uint8_t PIN_SPI_SCK  = 12;
  constexpr uint8_t PIN_SPI_MISO = 13;
  constexpr uint8_t PIN_SPI_MOSI = 11;
  constexpr uint8_t PIN_SD_CS    = 10;
  constexpr int8_t  PIN_OLED_CS  = 14;
  constexpr int8_t  PIN_OLED_DC  = 16;
  constexpr int8_t  PIN_OLED_RST = 21;   // -1: tied to the board's reset

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

  // SPI bus, shared by the microSD and an SPI display (DISPLAY_SPI): the
  // board's standard VSPI pins.
  constexpr uint8_t PIN_SPI_SCK  = 18;
  constexpr uint8_t PIN_SPI_MISO = 19;
  constexpr uint8_t PIN_SPI_MOSI = 23;
  constexpr uint8_t PIN_SD_CS    = 5;
  constexpr int8_t  PIN_OLED_CS  = 26;
  constexpr int8_t  PIN_OLED_DC  = 32;
  constexpr int8_t  PIN_OLED_RST = 33;   // -1: tied to the board's reset

#else
  // No silent fallback to the classic pinout: on an unlisted chip it would
  // boot and read sensors that were never connected.
  #error "Unsupported board. Use a classic ESP32, S2, S3 or C3, or add its pin block in Config.h by copying the closest one. See docs/HARDWARE.md."
#endif

// ===========================================================================
// I2C BUS
// ===========================================================================
// OLED, IMU and an I2C weather sensor share the bus. Addresses are found by
// themselves; two parts on the SAME address do not work (docs/HARDWARE.md
// has the map).
//
// 400 kHz: an OLED frame takes ~23 ms instead of ~90. Slower, 10 FPS do not
// fit.
constexpr uint32_t I2C_CLOCK_HZ = 400000;

// ===========================================================================
// DISPLAY
// ===========================================================================
// The pages are hand-drawn for 128x64: changing these does NOT re-flow them.
constexpr uint8_t SCREEN_WIDTH  = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;

// One character of the built-in font at size 1.
constexpr uint8_t FONT_CHAR_WIDTH = 6;

// ===========================================================================
// POWER FROM THE BIKE (optional)
// ===========================================================================
// On the bike's 12 V, turning the key off cuts the power in the middle of a
// recording: the last seconds and the session summary, lap times included,
// are lost. With the wiring in docs/HARDWARE.md ("Alimentazione dalla moto")
// - a divider from the switched 12 V to an ADC pin, and a capacitor that
// keeps the board alive for a moment - the firmware sees the key go off and
// closes the session first. Leave it commented out on USB power.
//
// It must be an ADC1 pin: ADC2 stops working while the WiFi is on.
// Classic ESP32: 32-39. S3/S2: 1-10. C3: 0-4 (check against your pinout).
// #define POWER_SENSE_PIN 34
constexpr float POWER_SENSE_DIVIDER = (100.0f + 22.0f) / 22.0f;  // (R1 + R2) / R2, in kOhm
constexpr float POWER_OFF_VOLTS     = 6.0f;  // below this the key is off; cranking stays above

// ===========================================================================
// IMU ORIENTATION - the first thing to set on a new build
// ===========================================================================
// Get it wrong and nothing fails: the numbers are just believable and false.
// The procedure, with the four checks to do box in hand, is in the README
// ("IMU orientation").

// STEP 1: how the breakout sits. "Up" is the component side facing the sky,
// and the chip's X axis (printed on most breakouts) points to the front.
#if !defined(IMU_MOUNT_UPRIGHT) && !defined(IMU_MOUNT_UPSIDE_DOWN) && \
    !defined(IMU_MOUNT_MANUAL)
  #define IMU_MOUNT_UPSIDE_DOWN   // fitted: board flipped over
// #define IMU_MOUNT_UPRIGHT      // components up, board flat
// #define IMU_MOUNT_MANUAL       // neither: set the two lines below by hand
#endif

// The same position told to each kind of chip:
//   IMU_BNO055_REMAP  the BNO055 turns its axes inside the chip, presets P0-P7
//   IMU_AXES          every other IMU: which chip axis points {forward, left,
//                     up}, 1 = X, 2 = Y, 3 = Z, minus when it points the
//                     other way. Flipped over about X, Y and Z reverse.
#if defined(IMU_MOUNT_UPSIDE_DOWN)
  #define IMU_BNO055_REMAP P5
  #define IMU_AXES {1, -2, -3}
#elif defined(IMU_MOUNT_UPRIGHT)
  #define IMU_BNO055_REMAP P1   // the chip's factory orientation
  #define IMU_AXES {1, 2, 3}
#elif defined(IMU_MOUNT_MANUAL)
  #define IMU_BNO055_REMAP P1   // <-- change here: P0 ... P7
  #define IMU_AXES {1, 2, 3}    // <-- and here
#endif

// STEP 2: flip the ones that come out reversed. A sign reverses a direction;
// two SWAPPED quantities (lean moves when the nose lifts) are a wrong step 1.
// These four apply to the BNO055. For any other IMU a reversed direction is
// fixed in IMU_AXES above: lean reversed -> change the sign of "left",
// pitch reversed -> the sign of "forward" and "up" together.
constexpr float SIGN_LEAN   =  1.0f;   // lean right must read positive
constexpr float SIGN_PITCH  = -1.0f;   // nose up must read as a wheelie
constexpr float SIGN_G_LONG =  1.0f;   // accelerating must read positive
constexpr float SIGN_G_LAT  = -1.0f;   // pushed left, the G dot must go left

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

#if !defined(DISPLAY_SSD1306) && !defined(DISPLAY_SSD1309) && \
    !defined(DISPLAY_SH1106)  && !defined(DISPLAY_SH1107)
  #error "Config.h: no display controller selected (see COMPONENTS)"
#endif

#if defined(SD_CARD) && defined(CONFIG_IDF_TARGET_ESP32C3)
  #error "Config.h: the C3 has no free pins for a microSD"
#endif

#if defined(DISPLAY_SPI) && defined(CONFIG_IDF_TARGET_ESP32C3)
  #error "Config.h: the C3 has no free pins for an SPI display"
#endif

#endif  // WHEELSTAT_CONFIG_H
