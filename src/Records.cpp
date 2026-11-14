#include "Records.h"

#include "Storage.h"

namespace records {

// "WSR3": bump it whenever AllTime or the Channel table changes.
constexpr uint32_t RECORDS_MAGIC = 0x57535233;

static AllTime allTime = {};
static bool    newRecord[CHANNEL_COUNT] = {};
static char    filePath[32] = "";

static void save() {
  if (!storage::saveStruct(filePath, allTime, RECORDS_MAGIC))
    Serial.println(F("ERROR: could not save the all-time records."));
}

void open(const char *path) {
  strlcpy(filePath, path, sizeof(filePath));
  allTime = AllTime{};  // start clean: the previous file may be another one

  if (storage::loadStruct(filePath, allTime, RECORDS_MAGIC)) {
    Serial.printf("All-time records loaded from %s (%u sessions).\n",
                  filePath, (unsigned)allTime.sessions);
  } else {
    Serial.printf("No valid records in %s: starting from zero.\n", filePath);
  }
}

const AllTime &current() { return allTime; }

void reset() {
  allTime = AllTime{};
  clearNewFlags();
  save();
}

void commitSession(const float *sessionMax, uint32_t minutes) {
  for (int c = 0; c < CHANNEL_COUNT; c++) {
    newRecord[c] = sessionMax[c] > allTime.channels[c];
    if (newRecord[c]) allTime.channels[c] = sessionMax[c];
  }

  // Career counters: a session with no record still counts.
  allTime.sessions++;
  allTime.totalMinutes += minutes;

  save();
}

bool isNew(uint8_t channel) { return newRecord[channel]; }

bool anyNew() {
  for (int c = 0; c < CHANNEL_COUNT; c++)
    if (newRecord[c]) return true;
  return false;
}

void clearNewFlags() {
  for (int c = 0; c < CHANNEL_COUNT; c++) newRecord[c] = false;
}

}  // namespace records
