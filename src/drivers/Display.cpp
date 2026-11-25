#include "Display.h"

#include <SPI.h>
#include <Wire.h>

#include "I2cBus.h"

// Same constructors for all four controllers (luck of the two libraries).
// On I2C the last -1 means "no reset pin".
#ifdef DISPLAY_SPI
DisplayDriver display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, PIN_OLED_DC, PIN_OLED_RST, PIN_OLED_CS);
#else
DisplayDriver display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#endif

static uint8_t address = 0;

bool displayBegin() {
#ifdef DISPLAY_SPI
  // SPI has no acknowledge: there is no asking whether the panel is there,
  // so it is taken as present. The bus is shared with the microSD.
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  #if defined(DISPLAY_SSD1306) || defined(DISPLAY_SSD1309)
    return display.begin(SSD1306_SWITCHCAPVCC);
  #else
    return display.begin(0, true);
  #endif
#else
  // The begin() of both libraries only fails when the buffer malloc fails:
  // they never query the panel, so with the screen unplugged they would
  // still report success. An ACK on the address can only come from a chip
  // that is really there - and it also says which of the two addresses
  // the module was built for (most use 0x3C, some 0x3D).
  if (i2c::present(0x3C)) address = 0x3C;
  else if (i2c::present(0x3D)) address = 0x3D;
  else return false;

  #if defined(DISPLAY_SSD1306) || defined(DISPLAY_SSD1309)
    // SWITCHCAPVCC: the panel derives the matrix high voltage from the 3.3 V
    // logic supply by itself.
    return display.begin(SSD1306_SWITCHCAPVCC, address);
  #else
    // reset = false: this library's reset puts the I2C clock back to its
    // default, throwing away the 400 kHz the 10 FPS depend on.
    return display.begin(address, false);
  #endif
#endif
}

const char *displayName() {
#if defined(DISPLAY_SSD1309)
  return "SSD1309";
#elif defined(DISPLAY_SH1106)
  return "SH1106";
#elif defined(DISPLAY_SH1107)
  return "SH1107";
#else
  return "SSD1306";
#endif
}
