#include "Sessions.h"

#include "SessionFormat.h"
#include "Storage.h"

namespace sessions {

using namespace sessionFormat;

static Entry listed[MAX_SESSIONS_LISTED];

bool isLogFile(const char *name) {
  return strncmp(name, LOG_PREFIX, strlen(LOG_PREFIX)) == 0;
}

bool isCurrentFormat(const char *name) {
  const char *dot = strrchr(name, '.');
  return isLogFile(name) && dot != nullptr && strcmp(dot, LOG_EXTENSION) == 0;
}

bool resolvePath(const char *requested, char *path, size_t size) {
  snprintf(path, size, "%s%s", requested[0] == '/' ? "" : "/", requested);

  // Session logs only, and no walking around the filesystem.
  if (!isLogFile(path + 1)) return false;
  if (strstr(path, "..") != nullptr) return false;
  return storage::sessions().exists(path);
}

bool open(const char *path, Opened &opened) {
  opened.file = storage::sessions().open(path, FILE_READ);
  if (!opened.file) return false;

  core::SessionHeader &header = opened.header;
  const size_t size = opened.file.size();

  // The sizes in the header must be the ones this firmware was built with:
  // a log is only ever read by the device that wrote it.
  bool valid = opened.file.read((uint8_t *)&header, sizeof(header)) == sizeof(header) &&
               header.magic == core::SESSION_MAGIC &&
               header.headerSize == sizeof(core::SessionHeader) &&
               header.sampleSize == sizeof(core::SessionSample) &&
               header.footerSize == sizeof(core::SessionFooter);
  if (!valid) {
    opened.file.close();
    return false;
  }
  header.track[sizeof(header.track) - 1] = '\0';

  // The summary, if there is one, is the last footerSize bytes.
  opened.hasFooter = false;
  if (size >= (size_t)header.headerSize + header.footerSize) {
    uint32_t magic = 0;
    opened.file.seek(size - header.footerSize);
    opened.hasFooter = opened.file.read((uint8_t *)&magic, sizeof(magic)) == sizeof(magic) &&
                       magic == core::SESSION_FOOTER_MAGIC;
  }

  opened.sampleCount = core::sampleCountFor(size, header, opened.hasFooter);
  opened.file.seek(header.headerSize);
  return true;
}

bool readFooter(Opened &opened, core::SessionFooter &footer) {
  if (!opened.hasFooter) return false;
  opened.file.seek(opened.file.size() - opened.header.footerSize);
  return opened.file.read((uint8_t *)&footer, sizeof(footer)) == sizeof(footer);
}

void readInfo(const char *nameOrPath, Info &info) {
  info.found      = false;
  info.lapCount   = 0;
  info.durationMs = 0;
  strlcpy(info.track, FREE_TRACK, sizeof(info.track));

  char path[32];
  snprintf(path, sizeof(path), "%s%s", nameOrPath[0] == '/' ? "" : "/", nameOrPath);

  Opened opened;
  if (!open(path, opened)) return;
  strlcpy(info.track, opened.header.track, sizeof(info.track));

  // Static: 860 bytes are too many for the stack.
  static core::SessionFooter footer;
  if (readFooter(opened, footer)) {
    info.found      = true;
    info.durationMs = footer.durationMs;
    info.lapCount   = footer.lapCount > MAX_LAPS ? MAX_LAPS : footer.lapCount;
    for (int lap = 0; lap < info.lapCount; lap++) info.lapTimes[lap] = footer.lapMs[lap];
  }
  opened.file.close();
}

int list(const Entry *&entries) {
  entries = listed;
  int count = 0;

  File root = storage::sessions().open("/");
  if (!root) return 0;

  File file = root.openNextFile();
  while (file && count < MAX_SESSIONS_LISTED) {
    const char *name = file.name();  // with or without "/" depending on the core
    if (name[0] == '/') name++;

    if (!file.isDirectory() && isCurrentFormat(name)) {
      Info info;
      readInfo(name, info);

      Entry &entry = listed[count++];
      strlcpy(entry.name, name, sizeof(entry.name));
      strlcpy(entry.track, info.track, sizeof(entry.track));
      entry.lapCount = info.lapCount;
    }
    file = root.openNextFile();
  }
  root.close();
  return count;
}

}  // namespace sessions
