// WheelStat - the settings page and the firmware update from the browser.

#include <Update.h>

#include "../Channels.h"
#include "../Recorder.h"
#include "../Settings.h"
#include "../Storage.h"
#include "../Tuning.h"
#include "../Version.h"
#include "../core/SessionCodec.h"
#include "../i18n/Text.h"
#include "WebInternal.h"

namespace web {

// ===========================================================================
// Settings
// ===========================================================================

static void printNumberField(Print &page, const char *label, const char *name, float value,
                             const core::Limits &limits, bool inG) {
  page.print(F("<label class='field'><span>"));
  page.print(label);
  page.print(inG ? F(" (G)") : F(" (&deg;)"));
  page.printf("</span><input type='number' name='%s' value='%.*f' min='%.*f' max='%.*f' "
              "step='%s' required></label>",
              name, inG ? 2 : 0, value, inG ? 2 : 0, limits.min, inG ? 2 : 0, limits.max,
              inG ? "0.05" : "1");
}

// Hours of recording the free space is worth at a given rate.
static float hoursLeftAt(uint8_t rateHz) {
  float bytesPerHour = 3600.0f * rateHz * sizeof(core::SessionSample);
  return storage::freeBytes() / bytesPerHour;
}

void handleSettings() {
  const core::Settings &current = settings::get();
  const String outcome = server.arg("saved");

  PageWriter page(server, CONTENT_HTML);
  beginPage(page, "WheelStat - " TR_W_NAV_SETTINGS, Tab::Settings);

  if (outcome == "1")
    page.print(F("<div class='card'><span style='color:var(--good)'>" TR_W_SET_SAVED "</span></div>"));
  else if (outcome == "wifi")
    page.print(F("<div class='card warn'>" TR_W_SET_WIFI_REJECTED "</div>"));

  page.print(F("<form class='stack' method='post' action='/settings/save'>"));

  // --- Access point ---
  page.print(F("<h2>" TR_W_SET_WIFI "</h2><div class='card'><div class='fields'>"
               "<label class='field'><span>" TR_W_SET_WIFI_NAME "</span><input name='ssid' value='"));
  printEscaped(page, current.wifiName);
  page.printf("' maxlength='%u' required></label>", (unsigned)core::WIFI_NAME_MAX);
  page.print(F("<label class='field'><span>" TR_W_SET_WIFI_PASSWORD "</span><input name='pass' value='"));
  printEscaped(page, current.wifiPassword);
  page.printf("' minlength='%u' maxlength='%u' autocomplete='off' required></label>",
              (unsigned)core::WIFI_PASSWORD_MIN, (unsigned)core::WIFI_PASSWORD_MAX);
  page.print(F("<span class='hint'>" TR_W_SET_WIFI_HELP "</span></div></div>"));

  // --- Session log ---
  page.print(F("<h2>" TR_W_SET_LOG "</h2><div class='card'><div class='fields'>"
               "<label class='field'><span>" TR_W_SET_LOG_RATE "</span>"));
  // The hours of recording left follow the number while it is typed: the
  // trade-off between detail and duration is the point of this setting.
  page.printf("<input type='number' name='rate' value='%u' min='%u' max='%u' step='1' required "
              "oninput=\"var r=+this.value;if(r>0)document.getElementById('hours').textContent=(%llu/(3600*r*%u)).toFixed(1)\">"
              "</label>",
              (unsigned)current.logRateHz, (unsigned)LOG_RATE_MIN_HZ, (unsigned)LOG_RATE_MAX_HZ,
              (unsigned long long)storage::freeBytes(), (unsigned)sizeof(core::SessionSample));
  page.print(F("<span class='hint'>" TR_W_SET_LOG_HELP_1 "<b id='hours'>"));
  page.printf("%.1f", hoursLeftAt(current.logRateHz));
  page.print(F("</b>" TR_W_SET_LOG_HELP_2 "</span></div></div>"));

  // --- Events and alert ---
  page.print(F("<h2>" TR_W_SET_EVENTS "</h2><div class='card'><div class='fields'>"));
  for (int e = 0; e < EVENT_COUNT; e++) {
    char name[6];
    snprintf(name, sizeof(name), "ev%d", e);
    printNumberField(page, eventInfo(e).label, name, current.eventThreshold[e],
                     core::eventLimits(e), eventInfo(e).inG);
  }
  page.print(F("<span class='hint'>" TR_W_SET_EVENTS_HELP "</span>"));
  printNumberField(page, TR_W_SET_LEAN_ALERT, "lean", current.leanAlertDeg,
                   core::leanAlertLimits(), false);
  page.print(F("<span class='hint'>" TR_W_SET_LEAN_ALERT_HELP
               "</span></div></div>"
               "<button type='submit' class='btn'>" TR_W_SET_SAVE "</button></form>"));

  page.print(F("<form method='post' action='/settings/reset' class='gap' "
               "onsubmit=\"return confirm('" TR_W_SET_CONFIRM_RESET "')\">"
               "<button type='submit' class='btn ghost small'>" TR_W_SET_RESET "</button></form>"));

  // --- Firmware ---
  page.print(F("<h2>" TR_W_UPDATE_TITLE "</h2><div class='card'><p>" TR_W_UPDATE_CURRENT " <b>"));
  page.print(FIRMWARE_VERSION);
  page.print(F("</b></p><p class='hint'>" TR_W_UPDATE_HELP "</p>"
               "<form class='stack' method='post' action='/update' enctype='multipart/form-data' "
               "onsubmit=\"return confirm('" TR_W_UPDATE_CONFIRM "')\">"
               "<input type='file' name='firmware' accept='.bin' required>"
               "<button type='submit' class='btn'>" TR_W_UPDATE_BUTTON "</button></form></div>"));

  endPage(page);
}

void handleSettingsSave() {
  if (rejectWhileRecording()) return;

  core::Settings updated = settings::get();

  // Name and password only change if both are acceptable as typed: silently
  // "fixing" a password would lock the rider out of the network.
  String name = server.arg("ssid");
  String password = server.arg("pass");
  name.trim();
  const bool wifiOk =
    core::validWifiName(name.c_str()) && core::validWifiPassword(password.c_str());
  if (wifiOk) {
    strlcpy(updated.wifiName, name.c_str(), sizeof(updated.wifiName));
    strlcpy(updated.wifiPassword, password.c_str(), sizeof(updated.wifiPassword));
  }

  // Numbers out of range are brought back in by settings::save().
  long rate = server.arg("rate").toInt();
  updated.logRateHz = (uint8_t)constrain(rate, (long)LOG_RATE_MIN_HZ, (long)LOG_RATE_MAX_HZ);
  for (int e = 0; e < EVENT_COUNT; e++) {
    char key[6];
    snprintf(key, sizeof(key), "ev%d", e);
    if (server.hasArg(key)) updated.eventThreshold[e] = server.arg(key).toFloat();
  }
  if (server.hasArg("lean")) updated.leanAlertDeg = server.arg("lean").toFloat();

  settings::save(updated);
  Serial.println(F("Settings changed from the website."));
  redirect(wifiOk ? "/settings?saved=1" : "/settings?saved=wifi");
}

void handleSettingsReset() {
  if (rejectWhileRecording()) return;
  settings::resetToDefaults();
  Serial.println(F("Settings reset to the defaults from the website."));
  redirect("/settings?saved=1");
}

// ===========================================================================
// Firmware update
// ===========================================================================
// The browser uploads the compiled sketch (WheelStat.ino.bin) and the device
// writes it into the spare application slot of the flash, then restarts
// from it. The running firmware is not touched until the new one has been
// received whole and checked, so an upload that breaks off halfway leaves
// the device as it was.

static const char *updateError = nullptr;  // null while everything is fine
static bool        updateBegun = false;

// The first bytes of an ESP32 application image, enough to turn away the
// wrong file before anything is written: the image magic, the chip it was
// built for, and the marker of the application descriptor - which a
// bootloader or a "merged" image of the whole flash does not have there.
static bool isFirmwareForThisChip(const uint8_t *data, size_t size) {
  constexpr size_t  CHIP_ID_OFFSET   = 12;
  constexpr size_t  APP_DESC_OFFSET  = 32;
  constexpr uint8_t IMAGE_MAGIC      = 0xE9;
  constexpr uint32_t APP_DESC_MAGIC  = 0xABCD5432;

  if (size < APP_DESC_OFFSET + 4 || data[0] != IMAGE_MAGIC) return false;

  uint16_t chipId = data[CHIP_ID_OFFSET] | (data[CHIP_ID_OFFSET + 1] << 8);
  uint32_t marker = (uint32_t)data[APP_DESC_OFFSET] | ((uint32_t)data[APP_DESC_OFFSET + 1] << 8) |
                    ((uint32_t)data[APP_DESC_OFFSET + 2] << 16) |
                    ((uint32_t)data[APP_DESC_OFFSET + 3] << 24);
  return chipId == CONFIG_IDF_FIRMWARE_CHIP_ID && marker == APP_DESC_MAGIC;
}

// Called by the server for every piece of the uploaded file.
void handleUpdateUpload() {
  HTTPUpload &upload = server.upload();

  switch (upload.status) {
    case UPLOAD_FILE_START:
      updateError = nullptr;
      updateBegun = false;
      if (recorder::isRecording()) {
        updateError = TR_E_STOP_RECORDING;
      } else {
        Serial.println(F("Firmware update: receiving..."));
        if (firmwareUploadCallback != nullptr) firmwareUploadCallback();
      }
      break;

    case UPLOAD_FILE_WRITE:
      if (updateError != nullptr) break;
      if (!updateBegun) {
        if (!isFirmwareForThisChip(upload.buf, upload.currentSize)) {
          updateError = TR_E_NOT_FIRMWARE;
          break;
        }
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          updateError = TR_E_UPDATE_FAILED;
          break;
        }
        updateBegun = true;
      }
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.abort();
        updateError = TR_E_UPDATE_FAILED;
      }
      break;

    case UPLOAD_FILE_END:
      // end(true) verifies the image and only then makes it the one to
      // boot from.
      if (updateError == nullptr && (!updateBegun || !Update.end(true)))
        updateError = TR_E_UPDATE_FAILED;
      break;

    case UPLOAD_FILE_ABORTED:
      if (updateBegun) Update.abort();
      updateError = TR_E_UPDATE_FAILED;
      break;
  }
}

// Called once the upload is over, to answer the browser.
void handleUpdateDone() {
  if (updateError != nullptr) {
    Serial.print(F("Firmware update refused: "));
    Serial.println(updateError);
    server.send(400, CONTENT_TEXT, updateError);
    return;
  }

  Serial.println(F("Firmware update done: restarting."));
  {
    PageWriter page(server, CONTENT_HTML);
    beginPage(page, "WheelStat - " TR_W_NAV_SETTINGS, Tab::Settings);
    page.print(F("<div class='card'><span style='color:var(--good)'>" TR_W_UPDATE_DONE
                 "</span></div>"));
    endPage(page);
  }
  delay(1000);  // let the answer reach the phone
  ESP.restart();
}

}  // namespace web
