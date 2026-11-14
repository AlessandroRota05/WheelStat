#include "Storage.h"

namespace storage {

static bool mounted = false;

bool begin() {
  mounted = LittleFS.begin(true);  // true: format if not initialised yet
  return mounted;
}

bool ok() { return mounted; }

bool ensureMounted() {
  if (!mounted) begin();
  return mounted;
}

size_t totalBytes() { return LittleFS.totalBytes(); }
size_t usedBytes()  { return LittleFS.usedBytes(); }
size_t freeBytes()  { return LittleFS.totalBytes() - LittleFS.usedBytes(); }

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
