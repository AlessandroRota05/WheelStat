// WheelStat - the dynamic pages of the website: sessions, tracks, compare.

#include "../Channels.h"
#include "../Gps.h"
#include "../Recorder.h"
#include "../Records.h"
#include "../Sessions.h"
#include "../Storage.h"
#include "../Tracks.h"
#include "../Util.h"
#include "../core/Geo.h"
#include "../i18n/Text.h"
#include "WebInternal.h"

namespace web {

static void printLapTime(Print &page, uint32_t ms) {
  char text[16];
  formatLapTime(ms, text, sizeof(text));
  page.print(text);
}

// ===========================================================================
// Sessions: the recorded files, and nothing else
// ===========================================================================

void handleSessions() {
  PageWriter page(server, CONTENT_HTML);
  beginPage(page, "WheelStat", Tab::Sessions);

  page.print(F("<h2>" TR_W_SESSIONS_TITLE "</h2><ul class='list'>"));

  // Track and lap count of each session, read from their summaries in one
  // scan; the loop below matches them by name.
  const sessions::Entry *entries;
  const int entryCount = sessions::list(entries);

  int found = 0;
  File root = LittleFS.open("/");
  if (root) {
    for (File file = root.openNextFile(); file; file = root.openNextFile()) {
      const char *name = file.name();  // with or without "/" depending on the core
      if (name[0] == '/') name++;
      if (file.isDirectory() || !sessions::isLogFile(name)) continue;

      // Logs left by firmware up to v9.16 are plain text: they can only be
      // downloaded as they are, or deleted.
      const bool current = sessions::isCurrentFormat(name);

      page.print(F("<li><div class='row'><span><b>"));
      page.print(name);
      page.print(F("</b><br><span class='dim'>"));
      for (int i = 0; i < entryCount; i++) {
        if (strcmp(entries[i].name, name) != 0) continue;
        printTrackName(page, entries[i].track);
        page.printf(" &middot; %d", entries[i].lapCount);
        page.print(F(TR_W_LAPS_WORD " &middot; "));
        break;
      }
      page.printf("%.1f KB", file.size() / 1024.0);
      if (!current) page.print(F(" &middot; " TR_W_OLD_FORMAT));
      page.print(F("</span></span><span>"));
      if (current) {
        page.printf("<a class='btn small' href='/session?f=%s'>", name);
        page.print(F(TR_W_CHARTS "</a> "));
      }
      page.printf("<a class='btn ghost small' href='/download?f=%s'>CSV</a> ", name);
      if (current) page.printf("<a class='btn ghost small' href='/gpx?f=%s'>GPX</a> ", name);
      page.printf("<a class='btn danger small' href='/delete?f=%s' ", name);
      page.print(F("onclick=\"return confirm('" TR_W_CONFIRM_DELETE_FILE "')\">"
                   TR_W_DELETE "</a></span></div></li>"));
      found++;
    }
    root.close();
  }
  if (found == 0) page.print(F("<li class='dim'>" TR_W_NO_SESSIONS "</li>"));
  page.print(F("</ul>"));

  // Fill level: the flash cannot be pulled out, better to see it here. The
  // gauge turns red in the last tenth, when a session may not fit any more.
  const size_t used = storage::usedBytes(), total = storage::totalBytes();
  const unsigned percent = total > 0 ? (unsigned)(used * 100 / total) : 0;
  page.print(F("<div class='card'><div class='row'><span class='dim'>" TR_W_STORAGE "</span>"));
  page.printf("<span>%u / %u KB</span></div>", (unsigned)(used / 1024), (unsigned)(total / 1024));
  page.printf("<div class='meter%s'><i style='width:%u%%'></i></div></div>",
              percent >= 90 ? " full" : "", percent);
  page.print(F("<p><a href='/tracks'>" TR_W_TRACKS_AND_RECORDS " &rarr;</a></p>"));

  if (recorder::isRecording())
    page.print(F("<div class='card'><span id='rec'>&#9679; REC</span>"
                 TR_W_REC_IN_PROGRESS "</div>"));

  endPage(page);
}

// ===========================================================================
// Tracks: leaderboard, active track, records, creation, saved tracks
// ===========================================================================

static int countSessionsOn(const sessions::Entry *entries, int count, const char *trackName) {
  int matches = 0;
  for (int i = 0; i < count; i++)
    if (strcmp(entries[i].track, trackName) == 0) matches++;
  return matches;
}

// Inline SVG of the track shape: the trace of its reference lap, in a flat
// local projection from the finish line (like the live GPS trace). The
// points do not change between requests, so no canvas or JavaScript is
// needed. False if the track has no reference lap yet.
static bool printTrackShape(Print &page, const tracks::Track &track) {
  if (track.referencePoints < 2) return false;

  const laps::Gate &finish = track.course.finish;
  auto local = [&](const core::TracePoint &point) {
    return core::toLocal(finish.lat, finish.lon, core::fromE7(point.latE7),
                         core::fromE7(point.lonE7));
  };

  // First pass for the bounding box, second to draw: the points are
  // projected twice instead of being kept in an array on the stack. The box
  // starts from the finish line (the origin), so it is always inside.
  float minX = 0, maxX = 0, minY = 0, maxY = 0;
  for (int i = 0; i < track.referencePoints; i++) {
    core::Point p = local(track.reference[i]);
    minX = fminf(minX, p.x);  maxX = fmaxf(maxX, p.x);
    minY = fminf(minY, p.y);  maxY = fmaxf(maxY, p.y);
  }

  // Everything is scaled into a 100x100 box with a 4 px margin; Y is
  // flipped so north is up.
  const float scale = 100.0f / fmaxf(1.0f, fmaxf(maxX - minX, maxY - minY));
  auto toX = [&](float x) { return (x - minX) * scale + 4; };
  auto toY = [&](float y) { return 108.0f - ((y - minY) * scale + 4); };

  page.print(F("<svg class='shape' viewBox='0 0 108 112' width='100%' height='90'><path d='M"));
  for (int i = 0; i < track.referencePoints; i++) {
    core::Point p = local(track.reference[i]);
    page.printf("%s%.1f,%.1f", i > 0 ? " L" : "", toX(p.x), toY(p.y));
  }
  page.print(F("' fill='none' stroke='var(--accent)' stroke-width='2' "
               "stroke-linejoin='round' stroke-linecap='round'/>"));
  page.printf("<circle cx='%.1f' cy='%.1f' r='3' fill='#fff'/></svg>", toX(0), toY(0));
  return true;
}

// A Track is about 2 KB: the pages that walk through the saved ones share
// this one buffer instead of putting it on the stack.
static tracks::Track scratchTrack;

static void printLeaderboard(Print &page, const sessions::Entry *entries, int sessionCount) {
  // First pass over the tracks just for these two figures; the full list
  // further down reads the same files again. Simpler than keeping every
  // Track in RAM (~2 KB each) for a page that is opened rarely.
  uint32_t fastestMs = 0;
  char     fastestTrack[sizeof(tracks::Track::name)] = "";
  char     busiestTrack[sizeof(tracks::Track::name)] = "";
  int      busiestSessions = 0;

  for (int id = 1; id <= MAX_TRACKS; id++) {
    tracks::Track &track = scratchTrack;
    if (!tracks::load(id, track)) continue;

    if (track.bestLapMs > 0 && (fastestMs == 0 || track.bestLapMs < fastestMs)) {
      fastestMs = track.bestLapMs;
      strlcpy(fastestTrack, track.name, sizeof(fastestTrack));
    }

    int sessionsHere = countSessionsOn(entries, sessionCount, track.name);
    if (sessionsHere > busiestSessions) {
      busiestSessions = sessionsHere;
      strlcpy(busiestTrack, track.name, sizeof(busiestTrack));
    }
  }

  page.print(F("<h2>" TR_W_LEADERBOARD "</h2><div class='card'>"));
  if (fastestMs == 0 && busiestSessions == 0) {
    page.print(F(TR_W_NO_DATA_YET));
  } else {
    if (fastestMs > 0) {
      page.print(F(TR_W_FASTEST_LAP_EVER "<b>"));
      printLapTime(page, fastestMs);
      page.print(F("</b>" TR_W_ON_TRACK));
      printEscaped(page, fastestTrack);
    }
    if (busiestSessions > 0) {
      if (fastestMs > 0) page.print(F("<br>"));
      page.print(F(TR_W_MOST_VISITED "<b>"));
      printEscaped(page, busiestTrack);
      page.printf("</b> (%d", busiestSessions);
      page.print(F(TR_W_SESSIONS_WORD ")"));
    }
  }
  page.print(F("</div>"));
}

static void printActiveTrack(Print &page) {
  page.print(F("<h2>" TR_W_ACTIVE_TRACK "</h2><div class='card'>"));
  if (!tracks::hasActive()) {
    page.print(F(TR_W_FREE_MODE " <span class='dim'>" TR_W_NO_TRACK_SELECTED "</span></div>"));
    return;
  }

  const tracks::Track &track = tracks::active();
  const int checkpoints = track.course.checkpointCount;
  page.print(F("<b>"));
  printEscaped(page, track.name);
  page.print(F("</b> <span class='badge on'>" TR_W_ACTIVE "</span>"));

  // Checkpoints are managed only on the active track: adding one means
  // being physically there with a GPS fix, like creating the track.
  page.printf("<div class='gap'>%d", checkpoints);
  page.print(checkpoints == 1 ? F(TR_W_CHECKPOINT_ONE) : F(TR_W_CHECKPOINT_MANY));
  if (checkpoints < MAX_CHECKPOINTS) {
    if (gps::fix().valid)
      page.print(F(" &nbsp;<a class='btn ghost small' href='/tracks/checkpoint/add'>"
                   TR_W_ADD_CHECKPOINT "</a>"));
    else
      page.print(F(" <span class='dim'>" TR_W_NEED_FIX_CHECKPOINT "</span>"));
  }
  if (checkpoints > 0)
    page.print(F(" &nbsp;<a class='btn danger small' href='/tracks/checkpoint/clear' "
                 "onclick=\"return confirm('" TR_W_CONFIRM_CLEAR_CHECKPOINTS "')\">"
                 TR_W_CLEAR_CHECKPOINTS "</a>"));
  page.print(F("</div><div class='gap'><a class='btn ghost small' href='/tracks/free'>"
               TR_W_BACK_TO_FREE "</a></div></div>"));
}

// Channel records of the active context (the active track, or free mode).
// They live here and not on the Sessions page because they belong to that
// context, like the leaderboard and the lap records around them.
static void printRecords(Print &page) {
  const records::AllTime &best = records::current();

  page.print(F("<h2>" TR_W_RECORDS));
  if (tracks::hasActive()) {
    page.print(F(" &middot; "));
    printEscaped(page, tracks::active().name);
  }
  page.print(F("</h2><div class='card'><table>"));
  for (int c = 0; c < CHANNEL_COUNT; c++) {
    const ChannelInfo &channel = channelInfo(c);
    page.print(F("<tr><td>"));
    page.print(channel.label);
    page.print(F("</td><td class='num'><b>"));
    page.print(best.channels[c], channel.decimals);
    page.print(channel.unitHtml);
    page.print(F("</b></td></tr>"));
  }
  page.print(F("</table></div><div class='caption'><span>"));
  page.print(best.sessions);
  page.print(F(TR_W_SESSIONS_WORD " &middot; "));
  page.print(best.totalMinutes);
  page.print(F(TR_W_MINUTES_TOTAL "</span><a class='btn small danger' href='/records/reset' "
               "onclick=\"return confirm('" TR_W_CONFIRM_RESET_RECORDS "')\">"
               TR_W_RESET "</a></div>"));
}

// The finish line of a new track is the DEVICE's GPS position at the moment
// the request arrives - not the phone's.
static void printCreateForm(Print &page) {
  page.print(F("<h2>" TR_W_CREATE_TITLE "</h2><div class='card'><p>" TR_W_CREATE_HELP "</p>"));
  if (!gps::fix().valid) {
    page.print(F("<p class='danger'>" TR_W_NO_FIX_CREATE "</p>"));
  } else {
    page.printf("<form class='inline' action='/tracks/create'><input name='name' maxlength='%u' ",
                (unsigned)(sizeof(tracks::Track::name) - 1));
    page.print(F("placeholder='" TR_W_TRACK_NAME "' required>"
                 "<button type='submit' class='btn'>" TR_W_CREATE_HERE "</button></form>"));
  }
  page.print(F("</div>"));
}

// Every possible id is tried and only the valid files are kept: simpler
// than a separate index, and with MAX_TRACKS this small it costs nothing.
static void printSavedTracks(Print &page, const sessions::Entry *entries, int sessionCount) {
  page.print(F("<h2>" TR_W_SAVED_TRACKS "</h2><ul class='list'>"));

  int found = 0;
  for (int id = 1; id <= MAX_TRACKS; id++) {
    tracks::Track &track = scratchTrack;
    if (!tracks::load(id, track)) continue;
    found++;

    page.print(F("<li>"));
    if (!printTrackShape(page, track))
      page.print(F("<div class='dim' style='font-size:12px;padding:4px 0'>"
                   TR_W_NO_SHAPE "</div>"));

    page.print(F("<div class='row gap'><span><b>"));
    printEscaped(page, track.name);
    page.print(F("</b>"));
    if (id == tracks::activeId()) page.print(F(" <span class='badge on'>" TR_W_ACTIVE "</span>"));

    page.print(F("<br><span class='dim'>" TR_W_LAP_RECORD));
    if (track.bestLapMs > 0) printLapTime(page, track.bestLapMs);
    else page.print(F("--"));
    page.printf(" &middot; %d", countSessionsOn(entries, sessionCount, track.name));
    page.print(F(TR_W_SESSIONS_WORD));
    page.printf(" &middot; %d", track.course.checkpointCount + 1);
    page.print(F(TR_W_SECTORS_WORD "</span></span><span>"));

    page.printf("<a class='btn ghost small' href='/tracks/select?id=%d'>", id);
    page.print(F(TR_W_SELECT "</a> "));
    page.printf("<a class='btn danger small' href='/tracks/delete?id=%d' ", id);
    page.print(F("onclick=\"return confirm('" TR_W_CONFIRM_DELETE_TRACK "')\">"
                 TR_W_DELETE "</a></span></div></li>"));
  }
  if (found == 0) page.print(F("<li class='dim'>" TR_W_NO_TRACKS "</li>"));
  page.print(F("</ul>"));
}

void handleTracks() {
  PageWriter page(server, CONTENT_HTML);
  beginPage(page, "WheelStat - " TR_W_NAV_TRACKS, Tab::Tracks);

  // One scan of the sessions, reused by the leaderboard and by the session
  // count of every track.
  const sessions::Entry *entries;
  int sessionCount = sessions::list(entries);

  printLeaderboard(page, entries, sessionCount);
  printActiveTrack(page);
  printRecords(page);
  printCreateForm(page);
  printSavedTracks(page, entries, sessionCount);

  endPage(page);
}

// ===========================================================================
// Compare: lap times of two sessions side by side
// ===========================================================================
// It answers "am I improving?" without opening two CSVs in a spreadsheet.

static void printSessionOptions(Print &page, const sessions::Entry *entries, int count,
                                const char *selected) {
  for (int i = 0; i < count; i++) {
    page.printf("<option value='%s'%s>%s - ", entries[i].name,
                strcmp(entries[i].name, selected) == 0 ? " selected" : "", entries[i].name);
    printTrackName(page, entries[i].track);
    page.printf(" (%d", entries[i].lapCount);
    page.print(F(TR_W_LAPS_WORD ")</option>"));
  }
}

static uint32_t fastestLap(const sessions::Info &info) {
  uint32_t best = 0;
  for (int i = 0; i < info.lapCount; i++)
    if (best == 0 || info.lapTimes[i] < best) best = info.lapTimes[i];
  return best;
}

// One table cell with a lap time; the faster of the two is highlighted, so a
// row-by-row comparison reads at a glance.
static void printTimeCell(Print &page, bool present, uint32_t ms, bool wins) {
  page.print(wins ? F("<td class='num win'>") : F("<td class='num'>"));
  if (present) printLapTime(page, ms);
  else page.print(F("--"));
  page.print(F("</td>"));
}

static void printComparison(Print &page, const char *nameA, const char *nameB) {
  sessions::Info a = {}, b = {};
  sessions::readInfo(nameA, a);
  sessions::readInfo(nameB, b);

  if (!a.found || !b.found || a.lapCount == 0 || b.lapCount == 0) {
    page.print(F("<div class='card'>" TR_W_COMPARE_NO_LAPS "</div>"));
    return;
  }

  if (strcmp(a.track, b.track) != 0) {
    page.print(F("<div class='card warn'>" TR_W_DIFFERENT_TRACKS));
    printTrackName(page, a.track);
    page.print(F(" / "));
    printTrackName(page, b.track);
    page.print(F(TR_W_DIFFERENT_TRACKS_END "</div>"));
  }

  page.print(F("<h2>" TR_W_LAP_TIMES "</h2><div class='card'><table>"
               "<tr><td class='dim'>" TR_W_LAP "</td>"
               "<td class='dim num'>A</td><td class='dim num'>B</td></tr>"));

  int rows = max(a.lapCount, b.lapCount);
  for (int i = 0; i < rows; i++) {
    bool hasA = i < a.lapCount, hasB = i < b.lapCount;
    bool both = hasA && hasB;

    page.printf("<tr><td>%d</td>", i + 1);
    printTimeCell(page, hasA, a.lapTimes[i], both && a.lapTimes[i] < b.lapTimes[i]);
    printTimeCell(page, hasB, b.lapTimes[i], both && b.lapTimes[i] < a.lapTimes[i]);
    page.print(F("</tr>"));
  }

  uint32_t bestA = fastestLap(a), bestB = fastestLap(b);
  page.print(F("<tr class='total'><td>" TR_W_BEST "</td>"));
  printTimeCell(page, true, bestA, bestA < bestB);
  printTimeCell(page, true, bestB, bestB < bestA);
  page.print(F("</tr></table></div>"));
}

void handleCompare() {
  String selectedA = server.arg("a");
  String selectedB = server.arg("b");

  PageWriter page(server, CONTENT_HTML);
  beginPage(page, "WheelStat - " TR_W_NAV_COMPARE, Tab::Compare);

  const sessions::Entry *entries;
  int count = sessions::list(entries);

  page.print(F("<h2>" TR_W_COMPARE_TITLE "</h2><div class='card'>"));

  // Empty state: without two sessions there is nothing to compare, better
  // to say so than to draw two empty selects and a button leading nowhere.
  if (count < 2) {
    page.print(F(TR_W_COMPARE_NEED_TWO));
    page.print(count == 0 ? F(TR_W_COMPARE_NONE) : F(TR_W_COMPARE_ONE));
    page.print(F("</div>"));
    endPage(page);
    return;
  }

  page.print(F("<form class='stack' action='/compare'>"
               "<label class='dim'>" TR_W_SESSION_A "</label><select name='a'>"));
  printSessionOptions(page, entries, count, selectedA.c_str());
  page.print(F("</select><label class='dim'>" TR_W_SESSION_B "</label><select name='b'>"));
  printSessionOptions(page, entries, count, selectedB.c_str());
  page.print(F("</select><button type='submit' class='btn'>" TR_W_COMPARE_BUTTON
               "</button></form></div>"));

  if (selectedA.length() > 0 && selectedB.length() > 0 && selectedA != selectedB)
    printComparison(page, selectedA.c_str(), selectedB.c_str());

  endPage(page);
}

}  // namespace web
