#pragma once

// WheelStat - declarations shared by the web source files. Internal to
// src/web: the rest of the firmware only includes Web.h.

#include <Arduino.h>
#include <WebServer.h>

#include "PageWriter.h"

namespace web {

extern WebServer server;

constexpr char CONTENT_HTML[] = "text/html; charset=utf-8";
constexpr char CONTENT_TEXT[] = "text/plain; charset=utf-8";

enum class Tab : uint8_t { Sessions, Live, Tracks, Compare, Settings };

// --- Shared helpers (Web.cpp) ----------------------------------------------
// Document head plus the brand/navigation bar common to every page, with
// the current tab highlighted.
void beginPage(Print &page, const char *title, Tab activeTab);
void endPage(Print &page);

// Prints text coming from the user or from files with the HTML special
// characters escaped.
void printEscaped(Print &out, const char *text);

// A track name as stored in a log, with the free-mode token localised.
void printTrackName(Print &out, const char *name);

// 303 redirect: after an action the page reloads already up to date.
void redirect(const char *location);

// Everything that touches the files is refused while recording: streaming a
// large file would stall the loop (and the telemetry) for seconds. Sends the
// error and returns true if a recording is in progress.
bool rejectWhileRecording();

// Resolves the "f" argument into the path of an existing session log. On
// failure it has already answered 404.
bool requestedLog(char *path, size_t size);

// --- Handlers ---------------------------------------------------------------
void handleStyle();            // WebAssets.cpp
void handleLive();

void handleSessions();         // WebPages.cpp
void handleTracks();
void handleCompare();

void handleSessionView();      // WebSession.cpp

void handleSettings();         // WebSettings.cpp
void handleSettingsSave();
void handleSettingsReset();
void handleUpdateUpload();
void handleUpdateDone();

void handleData();             // WebActions.cpp
void handleSessionData();
void handleDownload();
void handleGpx();
void handleDelete();
void handleRecordsReset();
void handleTrackCreate();
void handleTrackSelect();
void handleTrackFree();
void handleTrackDelete();
void handleCheckpointAdd();
void handleCheckpointsClear();

// Set by web::onFirmwareUpload().
extern void (*firmwareUploadCallback)();

}  // namespace web
