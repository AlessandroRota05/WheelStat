#pragma once

/*
  WheelStat - src/web/Web.h
  ===========================================================================
  The website served from the device's own access point
  (http://192.168.4.1): session list with charts and CSV/GPX download, live
  telemetry, track management, session comparison, settings and firmware
  update.

  The phone is connected to the bike's access point and has NO internet, so
  the pages use no external library or font.

    Web.cpp          server, routes, access point, shared page chrome
    WebAssets.cpp    stylesheet and the live page (HTML + JavaScript)
    WebPages.cpp     sessions, tracks and compare pages
    WebSession.cpp   the session viewer (charts and map of one session)
    WebSettings.cpp  settings page and firmware update
    WebActions.cpp   JSON telemetry, downloads and every state-changing URL
    PageWriter.*     chunked response writer
  ===========================================================================
*/

#include <Arduino.h>
#include <IPAddress.h>

namespace web {

// Registers the routes. The access point stays off until start().
void begin();

// Access point + HTTP server on and off.
bool start();
void stop();
bool isActive();

// Serves pending requests; does nothing while the access point is off.
void handleClient();

IPAddress address();
int clientCount();

// Called when a firmware upload begins. The device does nothing else until
// it ends, so this is the moment to say so on the display.
void onFirmwareUpload(void (*callback)());

}  // namespace web
