#pragma once

/*
  WheelStat - src/Storage.h
  ===========================================================================
  The internal flash (LittleFS), and the small files the firmware keeps on
  it as fixed-size binary structs guarded by a magic number. Session logs
  have their own reader and writer (src/Sessions, src/Recorder).
  ===========================================================================
*/

#include <Arduino.h>
#include <LittleFS.h>

namespace storage {

// Mounts the partition, formatting it on the very first boot.
bool begin();
bool ok();

// Retries the mount if it failed at boot. True if storage is usable.
bool ensureMounted();

size_t totalBytes();
size_t usedBytes();
size_t freeBytes();

bool readExact(const char *path, void *out, size_t size);
bool writeAll(const char *path, const void *data, size_t size);

// Binary structs are written and read back as they sit in RAM: no text
// parsing, no format errors. The first field must be `uint32_t magic`,
// which also carries the format version: change the struct, change the
// magic, and old files stop validating instead of yielding garbage.
//
// Size AND signature are both checked, which covers truncated, corrupted
// or older-format files. `out` is left untouched on failure.
template <typename T>
bool loadStruct(const char *path, T &out, uint32_t magic) {
  T loaded;
  if (!readExact(path, &loaded, sizeof(T)) || loaded.magic != magic) return false;
  out = loaded;
  return true;
}

// Rewrites the whole file. It happens a few times per session: negligible
// flash wear.
template <typename T>
bool saveStruct(const char *path, T &value, uint32_t magic) {
  value.magic = magic;
  return writeAll(path, &value, sizeof(T));
}

}  // namespace storage
