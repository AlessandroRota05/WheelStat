#include "Display.h"

#include <Wire.h>

// Same constructor for all four controllers (luck of the two libraries, not
// a guarantee: an SPI panel would need another). The last -1 means "no
// reset pin".
DisplayDriver display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// The begin() of both libraries only fails when the buffer malloc fails:
// they never query the panel, so with the screen unplugged they would still
// report success. An ACK on the I2C address can only come from a chip that
// is really there.
static bool panelAnswers() {
  Wire.beginTransmission(I2C_ADDR_OLED);
  return Wire.endTransmission() == 0;
}

bool displayBegin() {
  if (!panelAnswers()) return false;

#if defined(DISPLAY_SSD1306) || defined(DISPLAY_SSD1309)
  // SWITCHCAPVCC: the panel derives the matrix high voltage from the 3.3 V
  // logic supply by itself.
  return display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR_OLED);
#else
  // reset = false: this library's reset puts the I2C clock back to its
  // default, throwing away the 400 kHz the 10 FPS depend on.
  return display.begin(I2C_ADDR_OLED, false);
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
