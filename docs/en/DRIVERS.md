# How the drivers work

How the firmware finds the components, how the drivers are organised and how to add one. For the list of what is compatible see [HARDWARE.md](HARDWARE.md); for the rest of the code [ARCHITECTURE.md](ARCHITECTURE.md).

*Italian version: [DRIVER.md](../DRIVER.md).*

## The idea

**Every component that can be recognised is recognised at boot.** Each category has a list of drivers; at power-up the firmware tries them in order and uses the first that answers. The rest of the firmware always calls the same functions (`weatherSensor::read()`, `imu::read()`...) without knowing which chip is behind them.

```
src/drivers/
  I2cBus.*            the few I2C transactions everybody needs
  WeatherSensor.*     the weather sensor contract and the list of candidates
  weather/            one file per chip family: Sensirion, Aht, Hdc1080,
                      Si70xx, Htu31, Am2320, Bme280, Dht
  Imu.*               the IMU contract and the list
  imu/                Bno055, Bno08x, Mpu6050, Icm20948, Lsm6ds
  GpsModule.*         GPS speed, family and rate
  gps/                Ublox, OtherFamilies (CASIC and Airoha)
  Display.*           the OLED panel
```

**One file per family**: adding a chip means adding a file, without touching the others. The family is the right unit because chips from one maker often share protocol and registers (SHT4x, SHT3x and SHTC3 talk the same way; MPU6050, MPU6500 and ICM-20602 have the same registers).

## The contract

Each family file defines a single `Driver` structure with three or four function pointers:

```cpp
// src/drivers/weather/WeatherDrivers.h
struct Driver {
  const char *(*detect)();                           // name if present, nullptr if not
  bool (*read)(float &temperature, float &humidity);
  uint32_t intervalMs;                                // how often to read it
};
```

`detect()` looks for the chip, prepares it and returns the name to show at boot. The category file (`WeatherSensor.cpp`, `Imu.cpp`) holds the list of drivers and calls `detect()` on each until one answers. Each file's internal functions are `static`: outside the file only its `Driver` exists.

The rules the signature alone does not tell (they are in the headers too):

1. **`detect()` must recognise the chip, not just an address that answers.** An ACK at address `0x40` can come from four different chips: it takes an identity register, or an answer whose CRC checks out.
2. **`weatherSensor::read()` gets its parameters already filled** with the last good values and overwrites only what it really read.
3. **Weather sensors read in two phases**: each call collects the measurement the previous call started, and starts a new one. No driver waits for the conversion (up to 80 ms on the AHT20) by blocking the loop that reads the IMU.
4. **`imu::read()` hands over the bike's angles** (`Kind::Angles`, only the BNO055) **or the raw data in the chip's axes** (`Kind::Raw`): accelerometer in m/s² with gravity, gyroscope in rad/s with its zero already removed. Axes and attitude filter are done by `src/Motion`, once for all.
5. **`name()`** appears on the boot screen: 11 characters at most.

## Recognition

The order matters where several chips share an address:

| Address | Who is there | How they are told apart |
|---|---|---|
| `0x40` | HDC1080, HTU31D, Si7021/HTU21D | HDC1080: manufacturer register `0x5449`. HTU31D: serial number with CRC. Si70xx: user register, then the electronic ID (only the Si7021 has it) for the name. Tried in this order, from the surest test to the weakest |
| `0x44` | SHT4x, SHT3x | The serial number is asked with the SHT4x's command: only an SHT4x answers with two words whose CRC checks out |
| `0x68` / `0x69` | ICM-20948, MPU family | ICM-20948: register 0 = `0xEA`. MPU: register `0x75` |
| `0x76` / `0x77` | BME280, BMP280 | Chip ID `0x60` or `0x58`. The BMP280 has no humidity: it is recognised and explained on the serial monitor, not used |

The **DHT** is tried last: it is not on the bus, and the only way to know it is there is to read it (about a second when it is missing).

For the **GPS** recognition is different because there is no address: NMEA is listened for at the common speeds, then the version is asked over the u-blox protocol, and the other families' commands are all sent (each ignores those it does not know). Finally the rate obtained is **measured**: the one number that matters, whatever the module did with the commands.

## Why without libraries

The weather sensors and the IMUs without fusion are written **without a library**: a few dozen lines each, from the datasheet. Three reasons:

- **Flash**: to recognise them, they all have to be in the firmware. A library per chip (with its dependencies) would cost tens of KB; the C3 is already at 95%.
- **Control**: none of these sensors' libraries offers two-phase reads that do not block the loop.
- **Checking**: the part that can go wrong silently - conversions, checksums and attitude - lives in `src/core/` (`SensorMath`, `Ahrs`, `GpsCommands`) and is tested on a PC against the datasheets' examples.

Libraries stay for the chips where writing it by hand does not pay: **BNO055** and **BNO085** (complex protocols), **DHT** (microsecond timings), the **displays** (Adafruit GFX draws the pages) and the **NMEA** parser (TinyGPS++).

## The choices left in `Config.h`

Only what cannot be asked of the component:

| Macro | Why it is not recognised |
|---|---|
| `DISPLAY_SSD1306` · `SH1106` · `SSD1309` · `SH1107` | Different controllers answer on the same address and have no readable identity register |
| `DISPLAY_SPI` | SPI has no acknowledge: a missing panel goes unnoticed |
| `IMU_WITH_BNO055` · `IMU_WITH_BNO08X` | They are recognised, but their libraries cost flash: included only when needed |
| `SD_CARD` / `NO_SD_CARD` | Same as above: FAT support costs ~50 KB |
| `IMU_MOUNT_*`, `IMU_AXES`, `SIGN_*` | How the box is mounted: no sensor knows that |
| `POWER_SENSE_PIN` | A divider has nothing to answer |

## Adding a weather sensor

Example: a Sensirion SHT85, which talks like the SHT3x but answers only at `0x44`.

**1. A file** `src/drivers/weather/Sht85.cpp` (or one more case in `Sensirion.cpp`, if the protocol is the same), copied from the closest family:

```cpp
namespace weatherSensor {
static const char *detect() { /* the chip's identity */ }
static bool read(float &temperature, float &humidity) { /* two phases */ }
const Driver SHT85 = { detect, read, 1000 };
}
```

**2. The conversion** in `src/core/SensorMath`, with a test in `test/test_core/` using the datasheet's example.

**3. One line** in `WeatherDrivers.h` (`extern const Driver SHT85;`) and one in the list in `WeatherSensor.cpp`, in the right place relative to chips with the same address.

**4. The check**: `pio test -e native` and compiling for every board.

For an IMU it is the same in `src/drivers/imu/`: `detect()` sets the chip up and measures the gyro's zero with `measureGyroBias()`, `read()` hands over `Kind::Raw`.

### The build matrix

A driver can be checked without the component only by compiling it. Every version is compiled in the combinations that change the compiled code:

| What varies | Values |
|---|---|
| Board | `esp32doit-devkit-v1` · `esp32s3` · `esp32s2` · `esp32c3` |
| Language | `LANG_IT` · `LANG_EN` |
| Display | `DISPLAY_SH1106`, `DISPLAY_SPI` and both together |
| IMU with library | `IMU_WITH_BNO08X` |
| microSD | `NO_SD_CARD` |
| Other | `POWER_SENSE_PIN`, `IMU_MOUNT_UPRIGHT` |

The weather sensors and the IMUs without a library have no combinations: they are always all compiled.

## Typical errors

| Error | Almost certain cause |
|---|---|
| `undefined reference to 'weatherSensor::XYZ'` | The driver is in the list but its file does not define the `Driver` with that name |
| `'X' was not declared in this scope` in a module | The `#include` of the header declaring it is missing |
| `#error "Config.h: no display controller selected"` | You commented out every `DISPLAY_*` macro |
| `#error "Config.h: the C3 has no free pins..."` | microSD or SPI display turned on for a C3 |
| `#error "Config.h: select only one language"` | `LANG_IT` and `LANG_EN` both on |
| `'TR_...' was not declared` | A text key exists in one language file but not in the other |
| Component present but its row has a cross at boot | Address taken by another chip, or wiring: the serial monitor says what was found |
