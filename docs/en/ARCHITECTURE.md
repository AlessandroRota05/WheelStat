# How the firmware is built

For anyone who wants to read or change the code. To use the device, the [README](../../README.md#english) is enough.

*Italian version: [ARCHITETTURA.md](../ARCHITETTURA.md).*

## The folder

```
WheelStat/
  WheelStat.ino        just setup() and loop(): they call src/App
  Config.h             what changes with the hardware: language, drivers, pins, IMU orientation
  platformio.ini       the same sketch for PlatformIO, and the test environment
  src/
    App                boot, main loop, buttons
    Tuning.h           thresholds, timings and limits that do not depend on the hardware
    Channels           the table of measured quantities and events
    Settings           the settings changed from the website
    Motion, Weather, Gps, Stats          sensors and filter
    LapTimer, Tracks, Records            laps, tracks, records
    Recorder, Sessions, SessionFormat    writing and reading sessions
    Storage, Buttons, Util, Version
    Power              the bike's 12 V: closes the session when the key is turned (optional)
    core/              logic without Arduino, tested on a PC
    drivers/           the components, recognised at boot: weather/, imu/, gps/ (DRIVERS.md)
    ui/                OLED pages
    web/               website
    i18n/              Italian and English text
  test/                tests of src/core
  tools/oled-preview/  the OLED screens drawn on a PC, for the documentation
  Media/               documentation images: oled/ and web/ (it, en), hardware/
  docs/                documents in Italian; docs/en/ the same in English
```

Arduino compiles everything it finds in `src/` by itself, recursively. The sketch folder must be named `WheelStat`.

## Code rules

- **A module is a namespace**, with its state in `static` variables of its `.cpp`. Each exists once, so a class would add nothing: the state is just as unreachable from outside, with no objects to pass around. A user of a module includes only its `.h`.
- **Dependencies go one way only.** `Tracks` knows `LapTimer` and `Records`, not the other way round: the lap timer reports an event and does not know tracks exist.
- **Config.h or Tuning.h?** *"Does it change if I fit different parts, or the same parts mounted differently?"* Yes: `Config.h`. No: `src/Tuning.h`.
- **Code and comments in English.** Comments give the reason for a choice, they do not restate the code. History lives only in [CHANGELOG.md](../CHANGELOG.md) (in Italian).
- **No `String` on hot paths.** Web pages go out in chunks from a fixed buffer (`web::PageWriter`) instead of being built in RAM.

## The main loop

`app::loop()` is a cooperative scheduler: every job has its own timer and none blocks the others.

| Order | Job | How often |
|---|---|---|
| 0 | `power::keyTurnedOff()` | 20 ms, only with `POWER_SENSE_PIN`: first, because once the key is off every millisecond counts |
| 1 | `weather::update()` | `weather::intervalMs()`: 1-2 s, set by the sensor found |
| 2 | `motion::update()` → `stats::sample()` | every pass: no peak is missed |
| 3 | `gps::update()` | every pass: NMEA must be drained at once |
| 4 | `laps::update()` → `tracks::onLapEvent()` | every pass, on the positions just read |
| 5 | buttons, `web::handleClient()` | every pass |
| 6 | `oled::render()` | 100 ms |
| 7 | telemetry on the serial monitor | 5 s |
| 8 | `recorder::update()` | every pass: takes the samples that are due |

Three things block the loop, and they are known: serving a web page (hundreds of ms), the page-change animation (~200 ms) and the summary screens. That is why lap times do not use `millis()` but the GPS receiver's clock (`core::GpsClock`), and why the session file places each sample in time by counting from the start.

## src/core: logic without hardware

Everything that can be decided without touching a sensor lives in `src/core/`, in C++ without `Arduino.h`. The firmware compiles it as it is; the tests compile it for the PC.

| File | What it holds | Used by |
|---|---|---|
| `Geo.h` | Flat local projection, cross products | Gate, Trace, website |
| `Gate` | Finish line and checkpoints as lines; learning the direction | LapLogic |
| `LapLogic` | Laps, sectors, best lap | `src/LapTimer` |
| `Trace` | A lap trace that thins itself out; delta against the reference | LapLogic, `src/Tracks` |
| `GpsClock` | Monotonic clock locked to GPS time | `src/Gps` |
| `PeakFilter` | Pothole filter, maxima, events | `src/Stats` |
| `SessionCodec` | Binary session format, CSV row | `src/Recorder`, `src/Sessions`, website |
| `SettingsRules` | Defaults and limits of the settings | `src/Settings` |
| `GripRisk.h` | Grip risk formula | `src/Weather`, CSV |
| `Ahrs` | Attitude from IMUs without fusion: Madgwick filter with the cornering and braking correction from the GPS speed | `src/Motion` |
| `AxisMap.h` | From the chip's axes to the bike's | `src/Motion` |
| `SensorMath` | Conversions and checksums of the sensors read without a library | `src/drivers/weather/` |
| `GpsCommands` | GPS commands: `$PCAS`/`$PAIR` sentences and UBX frames with their checksums | `src/drivers/gps/` |

The matching modules in `src/` are thin wrappers: they read the sensors and call the core.

### Tests

```bash
pio test -e native
```

Needs `g++` on the PATH. They live in `test/test_core/` and simulate a ride: the lap timer gets the positions a GPS would send on a circular track at constant speed, whose exact lap time is known; the attitude filter gets what an accelerometer and gyroscope would give in a corner at 20° and at 40° and under braking. The sensor conversions and the GPS command checksums are tested against the datasheets' examples. What the tests cannot cover is everything else: sensors, display, flash, WiFi. For that there is only the test on the device ([TESTING.md](TESTING.md)).

The drivers have no tests: they are checked by compiling every combination ([DRIVERS.md](DRIVERS.md#the-build-matrix)).

### The screens on a PC

`tools/oled-preview/` compiles the real code of `src/ui/` together with the real Adafruit GFX, with stand-ins for the other modules that describe a made-up ride. Every screen is saved and turned into the images in `Media/oled/`, in both languages:

```bash
powershell -File tools/oled-preview/build.ps1
```

Needs `g++`, Python with Pillow, and the GFX library where the Arduino IDE installs it. After changing a page, run it again: the README images stay the real ones. If a page starts using a module function the stand-ins do not have yet, linking fails and it gets added to `preview.cpp`.

## Files on the flash

A 1.4 MB LittleFS partition (the default 4 MB scheme, which also has the two application slots used by the WiFi update). With a microSD the sessions (`LOG_n.BIN`) go to the card; everything else stays here. Whatever reads or writes sessions goes through `storage::sessions()`, which returns one or the other.

| File | Format | Content |
|---|---|---|
| `LOG_n.BIN` | `core/SessionCodec.h` | One session |
| `TRACK_n.BIN` | `tracks::Track`, ~2 KB | Name, finish line, checkpoints, reference lap |
| `TRACK_n_REC.BIN` | `records::AllTime` | Channel records on that track |
| `RECORD.BIN` | `records::AllTime` | Channel records in free mode |
| `SETTINGS.BIN` | `core::Settings` | Settings |

The small files are structs written as they sit in RAM, with a **magic number** in the first field that also serves as a version (`storage::loadStruct`). Whoever changes a struct changes its magic: old files stop validating and the defaults come back, instead of misaligned data being read.

### A session

```
SessionHeader   56 bytes   signature, sizes, rate, track name, thresholds
SessionSample   24 bytes   × N, at the log rate
SessionFooter  860 bytes   maxima, events, laps and sectors
```

- A sample is the **peak** of lean, pitch and G over its interval after the pothole filter, with its sign: the maximum of the samples equals the session maximum.
- Samples wait in RAM and reach the flash every 5 seconds, with the file kept open.
- A file **without a footer** is a recording that lost power: header and samples are fine, only the summary is missing.
- The CSV does not exist on the flash: the website produces it on download.
- The Charts page decodes the same bytes in JavaScript (`src/web/WebSession.cpp`): change a struct and that must be updated too.

## The website

The ESP32 library's `WebServer` on an access point, with no internet: no external libraries or fonts.

| File | Content |
|---|---|
| `Web.cpp` | Server, routes, access point, shared header |
| `WebAssets.cpp` | Stylesheet, Live page |
| `WebPages.cpp` | Sessions, Tracks, Compare |
| `WebSession.cpp` | Charts page: map, charts, lap table with the ideal lap, two laps overlaid |
| `WebSettings.cpp` | Settings, firmware update |
| `WebActions.cpp` | Live JSON, downloads, actions |

**Captive portal.** With the WiFi on, a small DNS server also runs, answering every name with the device's address, and requests for a host that is not ours are sent to the home page. A phone that has just joined checks an address of its maker to see whether there is internet; getting our page instead, it shows "sign in to the network", which opens the website. Together with the QR code on OLED page 10, joining takes no typing at all.

Actions that change settings and the firmware upload are `POST`. While recording, anything that reads or writes files is refused (`rejectWhileRecording`).

**Two laps overlaid.** Two laps never have the same number of samples nor the same line, so they are not compared sample by sample but at the same *point* of the track: each is measured along its own path and cut into 300 equal fractions of its length, and at each fraction speed, lean and time of both are interpolated. The difference of the times at each point is the gap chart. It needs the GPS: a lap without positions cannot be overlaid.

**Trying a page without the device.** The body and script of the Charts page contain no translated text (it comes from the `T` dictionary), so they can be extracted from the source and opened in a browser with a fake session file. That is how it was checked.

## Text

All the text lives in `src/i18n/Text_it.h` and `Text_en.h` as macros, so it can be pasted into larger literals at compile time. Only one is compiled: the second language takes no flash.

| Prefix | Where it ends up | Constraints |
|---|---|---|
| `TR_O_` | OLED | ASCII only, 21 characters per row, 8 for the label of a data row |
| `TR_W_` | Website | UTF-8, no `'` or `"` (they end up inside HTML attributes and JavaScript) |
| `TR_E_` | Website error responses | Plain text |
| `TR_CH_` | Channel names | - |

A new key goes into both files: compiling in both languages is the check.

## Adding things

- **A channel** (a measured quantity): an entry in `Channel`, a row in the table in `Channels.cpp`, a line in `stats::sample()`. Maxima, records and summary pick it up by themselves. It changes `records::AllTime`: its magic must change.
- **A setting**: a field in `core::Settings` with its default and limits in `SettingsRules.cpp`, a field in the form in `WebSettings.cpp`, and the magic in `Settings.cpp`.
- **A language**: copy `Text_en.h`, add `LANG_xx` in `Config.h` and its branch in `Text.h`.
- **A driver**: [DRIVERS.md](DRIVERS.md).
