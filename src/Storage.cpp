#include "Storage.h"

#include "../Config.h"

#ifdef SD_CARD
#include <SD.h>
#include <SPI.h>
#endif

namespace storage {

static bool mounted = false;
static bool card = false;

// The card is tried once, at boot: a card put in later is seen at the next
// power-up. Hot-plugging during a recording would only add ways to lose it.
static void mountCard() {
#ifdef SD_CARD
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);   // harmless if the display started it
  card = SD.begin(PIN_SD_CS, SPI, 20000000) && SD.cardType() != CARD_NONE;
#endif
}

bool begin() {
  mounted = LittleFS.begin(true);  // true: format if not initialised yet
  mountCard();
  return mounted;
}

bool ok() { return mounted || card; }

bool ensureMounted() {
  if (!mounted) mounted = LittleFS.begin(true);
  return ok();
}

bool onCard() { return card; }

const char *name() { return card ? "microSD" : "LittleFS"; }

fs::FS &sessions() {
#ifdef SD_CARD
  if (card) return SD;
#endif
  return LittleFS;
}

uint64_t totalBytes() {
#ifdef SD_CARD
  if (card) return SD.totalBytes();
#endif
  return LittleFS.totalBytes();
}

uint64_t usedBytes() {
#ifdef SD_CARD
  if (card) return SD.usedBytes();
#endif
  return LittleFS.usedBytes();
}

uint64_t freeBytes() { return totalBytes() - usedBytes(); }

bool readExact(const char *path, void *out, size_t size) {
  if (!mounted) return false;
  File file = LittleFS.open(path, FILE_READ);
  if (!file) return false;
  size_t bytesRead = file.read((uint8_t *)out, size);
  file.close();
  return bytesRead == size;
}

bool writeAll(const char *path, const void *data, size_t size) {
  if (!mounted) return false;
  File file = LittleFS.open(path, FILE_WRITE);
  if (!file) return false;
  size_t written = file.write((const uint8_t *)data, size);
  file.close();
  return written == size;
}

}  // namespace storage
