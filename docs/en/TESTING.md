# Testing

The first test of a new box, or a new firmware, from the bench to the track. The steps are in order: each one assumes the ones before it passed, so if one fails it is best to stop there. Keep the serial monitor open at 115200: every five seconds it prints what each sensor sees.

*Italian version: [COLLAUDO.md](../COLLAUDO.md).*

## On the bench

### 1. Boot

- [ ] The display shows the logo and then the list of the five components, each with a tick.
- [ ] The serial monitor ends with `=== SYSTEM READY ===`.

A cross is a real check, not a guess:

| Cross on | Usually |
|---|---|
| OLED | you would not see it: the display is off. The serial monitor says `Boot OLED...... ERROR` |
| ENV | no sensor answers: the DHT is not on GPIO 4, an I2C sensor is not on SDA/SCL, or there is no power. The serial monitor says if it found a BMP280 (no humidity) |
| IMU | SDA/SCL swapped, or an IMU with a library (BNO085) not turned on in `Config.h` |
| GPS | the module's TX is not on GPIO 16, or TX and RX are swapped. With the GPS present, the serial monitor prints family, speed and reports per second (for example `u-blox M8 at 115200 baud, 10 reports/s`) |
| MEM | flash partition: upload again choosing a scheme with SPIFFS/LittleFS |

### 2. Calibration

- [ ] With the BNO055 the magnetometer screen appears: move the box drawing a figure-eight in the air until `MAG` reaches 3/3. On the first day it can take a minute.
- [ ] The other IMUs need no calibration, but **the box must stay still during the boot screen**: the gyroscope's zero is measured in that second. The serial monitor prints it (`Gyro bias`): standing still it should be within a few degrees per second.

### 3. IMU orientation

- [ ] The four checks in the table in the [README](../../README.md#en-orientation): lean right positive, wheelie with the nose up, dot up when pushed forward, left when pushed left.

**Without this check everything else can look right and be wrong.**

### 4. Buttons and pages

- [ ] UP and DOWN change page, and the new page comes in from the right side.
- [ ] The stretch under the title moves one slot with each page.
- [ ] OK on page 0, with the box still and level, shows `CALIBRATION OK` and lean goes to 0.
- [ ] LOG brings up a blinking dot top right with the time running; LOG again stops the recording and shows the three summary screens.

### 5. Weather

- [ ] Temperature and humidity match the room. Breathe on the sensor: humidity rises within a few seconds.

### 6. GPS

Do it outdoors, or at least next to a window.

- [ ] On the GPS page the locked satellites go up. A new module's first fix can take a few minutes; later ones under a minute.
- [ ] With a fix, the speed standing still is 0 or nearly, and latitude and longitude match the place (check on a map).
- [ ] In the top bar the GPS ring gets its centre dot once there is a fix. The chequered flag only appears after you set a finish line: that is normal.

### 7. WiFi and website

- [ ] OK on the WiFi page: the network name, the password and `192.168.4.1` appear.
- [ ] DOWN goes to the page with the QR code. Pointing the camera at it, the phone offers to join the network; once joined, it offers to "sign in to the network" and opens the website.
- [ ] Alternatively, the phone joins by hand with that password and opens `http://192.168.4.1`.
- [ ] The Live page says `Connected`, and tilting the box changes the lean at once.
- [ ] Walk away until the network drops, or turn off the phone's WiFi: after a moment `Connection lost` appears.

### 8. Recording and sessions

- [ ] Record a couple of minutes moving the box, then stop.
- [ ] The website's Sessions page lists the new file; Charts opens the path (if there was a fix) and the charts; sliding a finger along a chart, the box above shows the values.
- [ ] Download the CSV and the GPX: the CSV opens in a spreadsheet, the GPX in Google Earth or similar.
- [ ] While recording, opening the charts, downloading or saving the settings answers that recording must be stopped first: that is on purpose.

### 9. Settings and update

- [ ] Change the samples per second and save: the page says it is saved, and after a restart the value is still there.
- [ ] Update the firmware from the website with `WheelStat.ino.bin` (Arduino IDE: *Sketch → Export Compiled Binary*). The display shows `UPDATING...`, then the board restarts; sessions and settings are still there.
- [ ] Try uploading a file that is not a firmware (for example a photo renamed `.bin`): it must be refused without restarting.

### 10. Turning off from the key (only with `POWER_SENSE_PIN`)

With a 12 V bench supply in place of the bike, wired as in [HARDWARE.md](HARDWARE.md#power-from-the-bike-).

- [ ] The serial monitor prints about `Power : 12.0 V` (the supply's voltage). If it is far off, check the divider resistors.
- [ ] Start a recording and switch off the supply: the display has time to show `SESSION SAVED`.
- [ ] Power up again: that session's Charts do **not** show the "cut short with no summary" warning.

## On the bike

### 11. Mounting

- [ ] The box is fixed rigidly: if it wobbles, G and lean measure the mount's vibration.
- [ ] Bike upright and level (on the centre stand, or held by someone): OK on page 0 zeroes lean and pitch. It must be redone at every power-up.

### 12. Laps

- [ ] Standing on the finish line with a fix, create a track from the website (or press OK on the Laps page).
- [ ] The first pass above 15 km/h starts the stopwatch; from the second, laps are counted.
- [ ] The times match a hand stopwatch to within a few tenths.
- [ ] From the lap after the first complete one, the delta appears on the Laps page: `-` when you are faster than the reference lap (in reverse video on the display), `+` when slower.
- [ ] With two splits added, the session's Charts table shows the sectors and the ideal lap.

## If something is wrong

Note down, for whoever has to understand the problem:

- the step that fails and what happens instead;
- the serial monitor text from boot (the `Boot ...` lines above all);
- a photo of the display;
- if it is about a recording, the `LOG_n.BIN` file (download it from the website as CSV).
