#pragma once

// WheelStat - English strings. Rules and explanations in Text.h.

#define TR_HTML_LANG "en"

// ===========================================================================
// OLED (ASCII only, mind the widths)
// ===========================================================================

// --- Boot and calibration ---
#define TR_O_BOOT_TITLE       "COMPONENTS"
#define TR_O_BOOT_WEATHER     "ENV"
#define TR_O_BOOT_STORAGE     "MEM"
#define TR_O_CAL_TITLE        "IMU CALIBRATION"
#define TR_O_CAL_SKIP         "[OK] skip"
#define TR_O_CAL_DONE         "CALIBRATED"
#define TR_O_CAL_ANY_KEY      "[any key] to start"

// --- Status bar ---
#define TR_O_STATUS_IDLE      "IDLE "

// --- Shared rows and short labels ---
#define TR_O_ROW_LEAN         "Lean"
#define TR_O_ROW_PITCH        "Wh/St"
#define TR_O_ROW_GLAT         "G lat"
#define TR_O_ROW_GLON         "G lon"
#define TR_O_LBL_RIGHT        "Rt "
#define TR_O_LBL_LEFT         "Lt "
#define TR_O_LBL_WHEELIE      "Wh "
#define TR_O_LBL_STOPPIE      "St "
#define TR_O_LBL_ACCEL        "Ac "
#define TR_O_LBL_BRAKE        "Br "
#define TR_O_SPEED            "Speed"
#define TR_O_STATUS           "Status"
#define TR_O_START            "Start"
#define TR_O_KEY_OK           "[OK] key"
#define TR_O_KEY_LOG          "[LOG] key"

// --- End-of-session summary ---
#define TR_O_SUM_MAX_1        "SESSION MAX 1/3"
#define TR_O_SUM_MAX_2        "SESSION MAX 2/3"
#define TR_O_SUM_EVENTS       "EVENT COUNT 3/3"
#define TR_O_SUM_CONTINUE     "[any key] next"
#define TR_O_SUM_DURATION     "Time "
#define TR_O_SUM_NEW          "* new!"
#define TR_O_SUM_KEY          "   [key]"
// Exactly 15 characters: the counts stay in a column
#define TR_O_EV_WHEELIES      "Wheelies     : "
#define TR_O_EV_STOPPIES      "Stoppies     : "
#define TR_O_EV_LEANS         "Leans        : "
#define TR_O_EV_BRAKES        "Hard braking : "
#define TR_O_EV_ACCELS        "Hard accel.  : "

// --- Pages ---
#define TR_O_LEAN_TITLE       "LEAN ANGLE"
#define TR_O_GRIP_DANGER      "! GRIP DANGER !"

#define TR_O_WEATHER_TITLE    "WEATHER & GRIP"
#define TR_O_AIR              "Air"
#define TR_O_HUMIDITY         "Humid."
#define TR_O_RISK             "Risk"

#define TR_O_G_TITLE          "G-FORCE"

#define TR_O_WHEELIE_TITLE    "WHEELIE"
#define TR_O_STOPPIE_TITLE    "STOPPIE"

#define TR_O_RECORDS_1        "RECORDS 1/2"
#define TR_O_RECORDS_2        "RECORDS 2/2"

#define TR_O_STORAGE_TITLE    "STORAGE"
#define TR_O_FREE_KB          " KB free"
#define TR_O_ERROR            "ERROR!"
#define TR_O_FILE             "File"
#define TR_O_MINUTES          "Minutes"
#define TR_O_PAUSED           "IDLE"

#define TR_O_WIFI_TITLE       "WIFI"
#define TR_O_NETWORK          "Net"
#define TR_O_PASSWORD         "Pass"
#define TR_O_SITE             "Site"
#define TR_O_CLIENTS          "Clients"
#define TR_O_OK_OFF           "  [OK] off"
#define TR_O_OFF              "OFF"

#define TR_O_GPS_TITLE        "GPS"
#define TR_O_GPS_MISSING      "MISSING"
#define TR_O_MODULE           "Module"
#define TR_O_GPS_SILENT       "silent"
#define TR_O_CHECK            "Check"
#define TR_O_GPS_CHECK_WIRES  "TX/RX/3.3V"
#define TR_O_NO_FIX           "NO FIX"
#define TR_O_SATS_LOCKED      " locked"
#define TR_O_WAIT             "Needs"
#define TR_O_OPEN_SKY         "open sky"

#define TR_O_LAPS_TITLE       "LAPS"
#define TR_O_LAPS_NOT_SET     "not set"
#define TR_O_LAPS_NO_FIX      "no GPS fix"
#define TR_O_LAP              "Lap"
#define TR_O_LAP_WAITING      "waiting"
#define TR_O_LAST             "Last"
#define TR_O_BEST             "Best"

// --- Popups ---
#define TR_O_POPUP_ZERO_OK    "CALIBRATION OK"
#define TR_O_POPUP_NO_FIX     "No GPS fix!"
#define TR_O_POPUP_FINISH_OK  "FINISH LINE OK"
#define TR_O_POPUP_STORAGE_FULL "STORAGE FULL"

// --- Standing messages ---
#define TR_O_UPDATING         "UPDATING..."

// ===========================================================================
// Channels (website tables)
// ===========================================================================
#define TR_CH_LEAN_RIGHT      "Lean right"
#define TR_CH_LEAN_LEFT       "Lean left"
#define TR_CH_WHEELIE         "Wheelie"
#define TR_CH_STOPPIE         "Stoppie"
#define TR_CH_GLAT_RIGHT      "Lateral G right"
#define TR_CH_GLAT_LEFT       "Lateral G left"
#define TR_CH_G_ACCEL         "Acceleration G"
#define TR_CH_G_BRAKE         "Braking G"
#define TR_CH_SPEED           "Speed"

// ===========================================================================
// Website (UTF-8, no ' or " characters)
// ===========================================================================

// --- Navigation and shared words ---
#define TR_W_NAV_SESSIONS     "Sessions"
#define TR_W_NAV_LIVE         "Live"
#define TR_W_NAV_TRACKS       "Tracks"
#define TR_W_NAV_COMPARE      "Compare"
#define TR_W_NAV_SETTINGS     "Settings"
#define TR_W_DELETE           "Delete"
#define TR_W_FREE_SHORT       "free"
#define TR_W_SESSIONS_WORD    " sessions"
#define TR_W_LAPS_WORD        " laps"
#define TR_W_SECTORS_WORD     " sectors"

// --- Sessions page ---
#define TR_W_SESSIONS_TITLE        "Stored sessions"
#define TR_W_CONFIRM_DELETE_FILE   "Delete permanently?"
#define TR_W_NO_SESSIONS           "No sessions found."
#define TR_W_STORAGE               "Storage"
#define TR_W_TRACKS_AND_RECORDS    "Tracks and records"
#define TR_W_REC_IN_PROGRESS       " in progress: charts, download and delete are available once recording is stopped."
#define TR_W_CHARTS                "Charts"
#define TR_W_OLD_FORMAT            "old format"

// --- Page of one session (charts) ---
// JavaScript dictionary: an object literal, strings in single quotes.
#define TR_W_SESSION_JS_DICT \
  "{loading:'Loading the session...'," \
  "error:'Cannot read the session (if a recording is running, stop it first).'," \
  "noGps:'No GPS position in this stretch',all:'Whole session'," \
  "lap:'Lap',laps:'Lap times',free:'free',duration:'Duration'," \
  "maxSpeed:'Top speed',maxLean:'Max lean',right:'right',left:'left',best:'Best lap'," \
  "hint:'Touch or slide along a chart to read the values at that point.'," \
  "range:'Show',speed:'Speed (km/h)',lean:'Lean (°)',leanShort:'lean'," \
  "pitch:'Wheelie (+) / stoppie (-) (°)',g:'G-forces',glat:'lateral'," \
  "glon:'longitudinal'," \
  "noFooter:'Recording cut short with no summary (power lost?): lap times are missing.'}"

// --- Live page ---
#define TR_W_LIVE_LEAN        "Lean &deg;"
#define TR_W_LIVE_PITCH       "Wheelie/Stoppie &deg;"
#define TR_W_LIVE_RISK        "Risk %"
#define TR_W_LIVE_GLAT        "Lateral G"
#define TR_W_LIVE_GLON        "Longitudinal G"
#define TR_W_LIVE_AIR         "Air &deg;C / Hum %"
#define TR_W_LIVE_SPEED       "GPS speed km/h"
#define TR_W_LIVE_DELTA       "Lap delta s"
#define TR_W_LIVE_TRACE       "Path ridden"
#define TR_W_LIVE_CLEAR       "Clear"
#define TR_W_LIVE_ANGLES      "Angles (&plusmn;60&deg;)"
#define TR_W_LIVE_LEGEND_LEAN "lean"
#define TR_W_LIVE_LEGEND_PITCH "wheelie(+) stoppie(-)"
#define TR_W_LIVE_GFORCES     "G-forces (&plusmn;1.5)"
#define TR_W_LIVE_LEGEND_LAT  "lateral"
#define TR_W_LIVE_LEGEND_LON  "longitudinal"
#define TR_W_LIVE_EVENTS      "Events"
#define TR_W_LIVE_LAPS        "Laps"
#define TR_W_LIVE_BACK        "&larr; Download sessions from storage"
// JavaScript dictionary of the live page: an object literal, strings in
// single quotes.
#define TR_W_LIVE_JS_DICT \
  "{wait:'Waiting for GPS points...',noFix:'NO FIX'," \
  "evW:'Wheelies',evS:'Stoppies',evL:'Leans',evB:'Hard braking'," \
  "evA:'Hard acceleration',lap:'Lap',last:'Last',best:'Best'," \
  "sectors:'Sectors',right:'R',left:'L'," \
  "online:'Connected',offline:'Connection lost'," \
  "noFinish:'Finish line not set (LAPS page on the display, or the Tracks tab)'}"

// --- Tracks page ---
#define TR_W_LEADERBOARD           "Leaderboard"
#define TR_W_NO_DATA_YET           "No data yet: record a few sessions with a finish line set."
#define TR_W_FASTEST_LAP_EVER      "Fastest lap ever: "
#define TR_W_ON_TRACK              " at "
#define TR_W_MOST_VISITED          "Most visited track: "
#define TR_W_ACTIVE_TRACK          "Active track"
#define TR_W_FREE_MODE             "Free mode"
#define TR_W_NO_TRACK_SELECTED     "(no track selected)"
#define TR_W_ACTIVE                "active"
#define TR_W_CHECKPOINT_ONE        " intermediate checkpoint set"
#define TR_W_CHECKPOINT_MANY       " intermediate checkpoints set"
#define TR_W_ADD_CHECKPOINT        "Add checkpoint here"
#define TR_W_NEED_FIX_CHECKPOINT   "(a GPS fix is needed to add more)"
#define TR_W_CONFIRM_CLEAR_CHECKPOINTS "Remove all checkpoints from this track?"
#define TR_W_CLEAR_CHECKPOINTS     "Remove checkpoints"
#define TR_W_BACK_TO_FREE          "Back to free mode"
#define TR_W_RECORDS               "Records"
#define TR_W_MINUTES_TOTAL         " minutes in total"
#define TR_W_CONFIRM_RESET_RECORDS "Reset all the all-time records?"
#define TR_W_RESET                 "Reset"
#define TR_W_CREATE_TITLE          "Create a new track here"
#define TR_W_CREATE_HELP           "Stop on the start/finish line with the GPS fix already locked, then give it a name: the finish line will be a line across the track at this point. It learns the direction of travel by itself on the first pass; the circuit shape and the reference lap for the delta are recorded on the first complete lap."
#define TR_W_NO_FIX_CREATE         "No GPS fix: wait for the lock before creating."
#define TR_W_TRACK_NAME            "Track name"
#define TR_W_CREATE_HERE           "Create here"
#define TR_W_SAVED_TRACKS          "Saved tracks"
#define TR_W_NO_SHAPE              "Shape not recorded yet (complete a lap)"
#define TR_W_LAP_RECORD            "Lap record: "
#define TR_W_SELECT                "Select"
#define TR_W_CONFIRM_DELETE_TRACK  "Delete this track and its records?"
#define TR_W_NO_TRACKS             "No saved tracks."

// --- Compare page ---
#define TR_W_COMPARE_TITLE         "Compare two sessions"
#define TR_W_COMPARE_NEED_TWO      "At least two recorded sessions are needed to compare them"
#define TR_W_COMPARE_NONE          " (there are none yet)."
#define TR_W_COMPARE_ONE           " (there is only one so far)."
#define TR_W_SESSION_A             "Session A"
#define TR_W_SESSION_B             "Session B"
#define TR_W_COMPARE_BUTTON        "Compare"
#define TR_W_COMPARE_NO_LAPS       "One of the two sessions has no timed laps to compare (a finish line must be set while recording)."
#define TR_W_DIFFERENT_TRACKS      "Different tracks ("
#define TR_W_DIFFERENT_TRACKS_END  "): the comparison still works but means less."
#define TR_W_LAP_TIMES             "Lap times"
#define TR_W_LAP                   "Lap"
#define TR_W_BEST                  "Best"

// --- Settings page ---
#define TR_W_SET_SAVED             "Settings saved."
#define TR_W_SET_WIFI_REJECTED     "Network name or password not valid: the previous ones were kept. Everything else was saved."
#define TR_W_SET_WIFI              "WiFi network"
#define TR_W_SET_WIFI_NAME         "Name"
#define TR_W_SET_WIFI_PASSWORD     "Password"
#define TR_W_SET_WIFI_HELP         "Name up to 12 characters, password 8 to 12, no accented letters and no spaces in the password: they must fit on one row of the display, where you can read them back. They apply the next time the WiFi is switched on."
#define TR_W_SET_LOG               "Recording"
#define TR_W_SET_LOG_RATE          "Samples per second"
#define TR_W_SET_LOG_HELP_1        "At this rate the free space is enough for about "
#define TR_W_SET_LOG_HELP_2        " hours of recording."
#define TR_W_SET_EVENTS            "Events and alert"
#define TR_W_SET_EVENTS_HELP       "Thresholds above which a manoeuvre is counted as an event."
#define TR_W_SET_LEAN_ALERT        "Maximum lean"
#define TR_W_SET_LEAN_ALERT_HELP   "Lean above which the display warns of danger on perfect tarmac; the grip risk lowers it."
#define TR_W_SET_SAVE              "Save"
#define TR_W_SET_RESET             "Restore the initial values"
#define TR_W_SET_CONFIRM_RESET     "Put thresholds and rate back to their initial values? Network name and password stay."
// Names of the event thresholds
#define TR_W_EV_WHEELIES           "Wheelie"
#define TR_W_EV_STOPPIES           "Stoppie"
#define TR_W_EV_LEANS              "Lean"
#define TR_W_EV_BRAKES             "Hard braking"
#define TR_W_EV_ACCELS             "Hard acceleration"

// --- Firmware update ---
#define TR_W_UPDATE_TITLE          "Firmware update"
#define TR_W_UPDATE_CURRENT        "Installed version:"
#define TR_W_UPDATE_HELP           "Pick the WheelStat.ino.bin file built for this board (in the Arduino IDE: Sketch, Export Compiled Binary). Sessions, tracks and settings are kept. Do not cut power during the upload: it takes about a minute, then the device restarts."
#define TR_W_UPDATE_CONFIRM        "Upload this firmware and restart the device?"
#define TR_W_UPDATE_BUTTON         "Upload and update"
#define TR_W_UPDATE_DONE           "Firmware updated: the device is restarting. Switch the WiFi back on from the display and reconnect."

// ===========================================================================
// Website error replies (plain text, UTF-8)
// ===========================================================================
#define TR_E_STOP_RECORDING   "Stop the recording before doing this."
#define TR_E_FILE_NOT_FOUND   "File not found."
#define TR_E_READ_ERROR       "Error reading from storage."
#define TR_E_NOT_A_SESSION    "This file is not a session this firmware version can read."
#define TR_E_NOT_FIRMWARE     "The file is not a firmware for this board: it takes WheelStat.ino.bin built for the same kind of ESP32. Nothing was written."
#define TR_E_UPDATE_FAILED    "Update failed: the previous firmware is still in place."
#define TR_E_PAGE_NOT_FOUND   "Page not found"
