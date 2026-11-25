#pragma once

/*
  WheelStat - src/Storage.h
  ===========================================================================
  Where things are kept. Two places:
    - the internal flash (LittleFS): the small files the firmware keeps as
      fixed-size binary structs guarded by a magic number - settings,
      tracks, records - always here;
    - the sessions: on a microSD when one is fitted (SD_CARD in Config.h)
      and a card is in at boot, otherwise on the internal flash as well.
  Session logs have their own reader and writer (src/Sessions,
  src/Recorder), which go through sessions().
  ===========================================================================
*/

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

namespace storage {

// Mounts the internal partition (formatting it on the very first boot) and
// looks for a card.
bool begin();
bool ok();

// Retries the mount if it failed at boot. True if storage is usable.
bool ensureMounted();

// Where the sessions go, and its name for the boot checklist.
fs::FS &sessions();
bool onCard();
const char *name();

// Of the sessions' storage. 64 bits: a microSD is larger than 4 GB.
uint64_t totalBytes();
uint64_t usedBytes();
uint64_t freeBytes();

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
