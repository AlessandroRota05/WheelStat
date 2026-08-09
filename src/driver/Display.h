#pragma once

/*
  WheelStat - src/driver/Display.h
  ===========================================================================
  Accensione del pannello OLED. Il tipo dell'oggetto (DisplayDriver) e i
  colori li sceglie Config.h in base alla macro attiva; qui resta solo la
  sequenza di accensione, che e' l'unica cosa davvero specifica del chip.

  Tutto il disegno sta in PagineOled.ino ed e' scritto in primitive
  Adafruit_GFX, comuni a qualunque pannello monocromatico: e' il motivo
  per cui questo file e' cosi' corto.

  Contratto: displayInit(), displayNome() + l'oggetto display - Config.h.

  Provato solo l'SSD1306. Gli altri tre compilano; da verificare al primo
  montaggio, in particolare l'indirizzo (0x3C oppure 0x3D).
  ===========================================================================
*/

#include "../../Config.h"

// Il costruttore e' lo stesso per tutti e quattro i controller (una
// fortuna delle due librerie, non una garanzia: un pannello SPI ne
// vorrebbe un altro). L'ultimo -1 significa "nessun pin di reset".
DisplayDriver display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Controlla che il pannello sia attaccato. Serve perche' le begin() di
// entrambe le librerie ritornano false solo quando fallisce la malloc del
// buffer: non interrogano il pannello, e a schermo staccato direbbero "OK"
// lo stesso. Un ACK sull'indirizzo I2C invece lo puo' dare solo un chip
// che c'e'. Wire.begin() l'ha gia' chiamato setup().
bool displayRisponde() {
  Wire.beginTransmission(I2C_ADDR_OLED);
  return Wire.endTransmission() == 0;
}

bool displayInit() {
  if (!displayRisponde()) return false;

#if defined(DISPLAY_SSD1306) || defined(DISPLAY_SSD1309)
  // SWITCHCAPVCC: il pannello ricava da se' la tensione alta della
  // matrice dai 3.3 V della logica.
  return display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR_OLED);
#else
  // reset = false: il reset di questa libreria rimette il clock I2C al
  // default, buttando via i 400 kHz da cui dipendono i 10 FPS.
  return display.begin(I2C_ADDR_OLED, false);
#endif
}

const __FlashStringHelper *displayNome() {
#if defined(DISPLAY_SSD1309)
  return F("SSD1309");   // stessa libreria dell'SSD1306, pannello diverso
#elif defined(DISPLAY_SH1106)
  return F("SH1106");
#elif defined(DISPLAY_SH1107)
  return F("SH1107");
#else
  return F("SSD1306");
#endif
}
