#include "Web.h"

#include <WiFi.h>

#include "../Recorder.h"
#include "../SessionFormat.h"
#include "../Settings.h"
#include "../i18n/Text.h"
#include "WebInternal.h"

namespace web {

WebServer server(80);

void (*firmwareUploadCallback)() = nullptr;

static bool active = false;

void begin() {
  server.on("/",                        handleSessions);
  server.on("/live",                    handleLive);
  server.on("/tracks",                  handleTracks);
  server.on("/compare",                 handleCompare);
  server.on("/session",                 handleSessionView);
  server.on("/settings",                handleSettings);
  server.on("/style.css",               handleStyle);
  server.on("/data",                    handleData);
  server.on("/session/data",            handleSessionData);
  server.on("/download",                handleDownload);
  server.on("/gpx",                     handleGpx);
  server.on("/delete",                  handleDelete);
  server.on("/records/reset",           handleRecordsReset);
  server.on("/tracks/create",           handleTrackCreate);
  server.on("/tracks/select",           handleTrackSelect);
  server.on("/tracks/free",             handleTrackFree);
  server.on("/tracks/delete",           handleTrackDelete);
  server.on("/tracks/checkpoint/add",   handleCheckpointAdd);
  server.on("/tracks/checkpoint/clear", handleCheckpointsClear);

  // POST only: the network password must not end up in an address, where
  // the browser history would keep it.
  server.on("/settings/save",  HTTP_POST, handleSettingsSave);
  server.on("/settings/reset", HTTP_POST, handleSettingsReset);
  server.on("/update",         HTTP_POST, handleUpdateDone, handleUpdateUpload);

  server.onNotFound([]() { server.send(404, CONTENT_TEXT, TR_E_PAGE_NOT_FOUND); });
}

bool start() {
  // The radio goes on first: the password may have to be generated right
  // now, and the random generator needs it (see settings::wifiPassword).
  WiFi.mode(WIFI_AP);
  const char *password = settings::wifiPassword();
  active = WiFi.softAP(settings::wifiName(), password);

  if (active) {
    server.begin();
    Serial.print(F("WiFi on -> http://"));
    Serial.print(WiFi.softAPIP());
    Serial.print(F("  network: "));
    Serial.print(settings::wifiName());
    Serial.print(F("  password: "));
    Serial.println(password);
  } else {
    Serial.println(F("ERROR: could not start the access point."));
  }
  return active;
}

void stop() {
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  active = false;
  Serial.println(F("WiFi off."));
}

bool isActive() { return active; }

void handleClient() {
  if (active) server.handleClient();
}

IPAddress address() { return WiFi.softAPIP(); }

int clientCount() { return WiFi.softAPgetStationNum(); }

void onFirmwareUpload(void (*callback)()) { firmwareUploadCallback = callback; }

// ===========================================================================
// Shared helpers
// ===========================================================================

static void navLink(Print &page, const char *href, const char *label, bool current) {
  page.print(F("<a href='"));
  page.print(href);
  page.print(current ? F("' class='on'>") : F("'>"));
  page.print(label);
  page.print(F("</a>"));
}

void beginPage(Print &page, const char *title, Tab activeTab) {
  page.print(F("<!DOCTYPE html><html lang='" TR_HTML_LANG "'><head><meta charset='utf-8'>"
               "<meta name='viewport' content='width=device-width,initial-scale=1'><title>"));
  page.print(title);
  page.print(F("</title><link rel='stylesheet' href='/style.css'></head><body>"
               "<div class='topbar'><div class='brand'>Wheel<span>Stat</span></div>"
               "<nav class='tabs'>"));
  navLink(page, "/",         TR_W_NAV_SESSIONS, activeTab == Tab::Sessions);
  navLink(page, "/live",     TR_W_NAV_LIVE,     activeTab == Tab::Live);
  navLink(page, "/tracks",   TR_W_NAV_TRACKS,   activeTab == Tab::Tracks);
  navLink(page, "/compare",  TR_W_NAV_COMPARE,  activeTab == Tab::Compare);
  navLink(page, "/settings", TR_W_NAV_SETTINGS, activeTab == Tab::Settings);
  page.print(F("</nav></div>"));
}

void endPage(Print &page) {
  page.print(F("</body></html>"));
}

void printEscaped(Print &out, const char *text) {
  for (const char *c = text; *c != '\0'; c++) {
    switch (*c) {
      case '&':  out.print(F("&amp;"));  break;
      case '<':  out.print(F("&lt;"));   break;
      case '>':  out.print(F("&gt;"));   break;
      case '"':  out.print(F("&quot;")); break;
      case '\'': out.print(F("&#39;"));  break;
      default:   out.write((uint8_t)*c); break;
    }
  }
}

void printTrackName(Print &out, const char *name) {
  if (strcmp(name, sessionFormat::FREE_TRACK) == 0) out.print(TR_W_FREE_SHORT);
  else printEscaped(out, name);
}

void redirect(const char *location) {
  server.sendHeader("Location", location);
  server.send(303);
}

bool rejectWhileRecording() {
  if (!recorder::isRecording()) return false;
  server.send(503, CONTENT_TEXT, TR_E_STOP_RECORDING);
  return true;
}

}  // namespace web
