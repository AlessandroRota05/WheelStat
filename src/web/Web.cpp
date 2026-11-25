#include "Web.h"

#include <DNSServer.h>
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

// Captive portal. Every name the phone looks up resolves to the device, and
// any address that is not ours is sent to the home page. A phone joining
// the network checks a known address of its maker to see whether there is
// internet; getting our page instead, it shows the "sign in to the network"
// prompt, which opens the site without typing 192.168.4.1.
static DNSServer dns;
constexpr uint16_t DNS_PORT = 53;

static void handleNotFound() {
  if (server.hostHeader() != WiFi.softAPIP().toString()) {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
    server.send(302, CONTENT_TEXT, "");
    return;
  }
  server.send(404, CONTENT_TEXT, TR_E_PAGE_NOT_FOUND);
}

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

  server.onNotFound(handleNotFound);
}

bool start() {
  // The radio goes on first: the password may have to be generated right
  // now, and the random generator needs it (see settings::wifiPassword).
  WiFi.mode(WIFI_AP);
  const char *password = settings::wifiPassword();
  active = WiFi.softAP(settings::wifiName(), password);

  if (active) {
    server.begin();
    dns.start(DNS_PORT, "*", WiFi.softAPIP());
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
  dns.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  active = false;
  Serial.println(F("WiFi off."));
}

bool isActive() { return active; }

void handleClient() {
  if (!active) return;
  dns.processNextRequest();
  server.handleClient();
}

IPAddress address() { return WiFi.softAPIP(); }

int clientCount() { return WiFi.softAPgetStationNum(); }

void onFirmwareUpload(void (*callback)()) { firmwareUploadCallback = callback; }

// ===========================================================================
// Shared helpers
// ===========================================================================

// Tab icons: 16x16 line drawings in the text colour, inline so the site
// needs no files or fonts from outside.
static const char ICON_SESSIONS[] PROGMEM = "<path d='M3 4h10M3 8h10M3 12h7'/>";
static const char ICON_LIVE[] PROGMEM     = "<path d='M1 8h3l2-5 4 10 2-5h3'/>";
static const char ICON_TRACKS[] PROGMEM   = "<path d='M3 15V2m0 1h9l-2 3 2 3H3'/>";
static const char ICON_COMPARE[] PROGMEM  = "<path d='M4 13V8m4 5V3m4 10V6'/>";
static const char ICON_SETTINGS[] PROGMEM =
  "<circle cx='8' cy='8' r='2.5'/><path d='M8 1.5v2M8 12.5v2M1.5 8h2M12.5 8h2M3.4 3.4l1.4 1.4"
  "M11.2 11.2l1.4 1.4M3.4 12.6l1.4-1.4M11.2 4.8l1.4-1.4'/>";

static void navLink(Print &page, const char *href, const char *icon, const char *label, bool current) {
  page.print(F("<a href='"));
  page.print(href);
  page.print(current ? F("' class='on'>") : F("'>"));
  page.print(F("<svg viewBox='0 0 16 16'>"));
  page.print(FPSTR(icon));
  page.print(F("</svg><span>"));
  page.print(label);
  page.print(F("</span></a>"));
}

void beginPage(Print &page, const char *title, Tab activeTab) {
  page.print(F("<!DOCTYPE html><html lang='" TR_HTML_LANG "'><head><meta charset='utf-8'>"
               "<meta name='viewport' content='width=device-width,initial-scale=1'><title>"));
  page.print(title);
  page.print(F("</title><link rel='stylesheet' href='/style.css'></head><body>"
               "<div class='topbar'><div class='brand'>Wheel<span>Stat</span></div>"
               "<nav class='tabs'>"));
  navLink(page, "/",         ICON_SESSIONS, TR_W_NAV_SESSIONS, activeTab == Tab::Sessions);
  navLink(page, "/live",     ICON_LIVE,     TR_W_NAV_LIVE,     activeTab == Tab::Live);
  navLink(page, "/tracks",   ICON_TRACKS,   TR_W_NAV_TRACKS,   activeTab == Tab::Tracks);
  navLink(page, "/compare",  ICON_COMPARE,  TR_W_NAV_COMPARE,  activeTab == Tab::Compare);
  navLink(page, "/settings", ICON_SETTINGS, TR_W_NAV_SETTINGS, activeTab == Tab::Settings);
  page.print(F("</nav></div><main>"));
}

void endPage(Print &page) {
  page.print(F("</main></body></html>"));
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
