/*
  CEVA/Hillcrest BNO085 and BNO086 at 0x4A (0x4B with DI high) - the
  successors of the BNO055. Optional (IMU_WITH_BNO08X in Config.h): its
  library, Adafruit BNO08x, takes about 10 KB of flash.

  The chip can fuse by itself, but its fusion does not know the bike's
  speed, and in a long corner no speed means no lean (see src/core/Ahrs.h).
  So it is used as a very good raw sensor instead: calibrated gyroscope and
  accelerometer, which the shared filter turns into attitude with the GPS
  speed. The chip also calibrates its gyro by itself and remembers it.
*/

#include "../../../Config.h"

#ifdef IMU_WITH_BNO08X

#include "ImuDrivers.h"

#include <Adafruit_BNO08x.h>

#include "../I2cBus.h"

namespace imu {
namespace bno08x_chip {

static Adafruit_BNO08x bno08x(-1);
static float   accel[3], gyro[3];
static bool    haveAccel = false, haveGyro = false;
static uint8_t accuracy = 0;

constexpr uint32_t REPORT_INTERVAL_US = 5000;   // 200 a second

static void enableReports() {
  bno08x.enableReport(SH2_ACCELEROMETER, REPORT_INTERVAL_US);
  bno08x.enableReport(SH2_GYROSCOPE_CALIBRATED, REPORT_INTERVAL_US);
}

static const char *detect() {
  bool found = false;
  for (uint8_t at : {0x4A, 0x4B})
    if (i2c::present(at) && bno08x.begin_I2C(at)) { found = true; break; }
  if (!found) return nullptr;
  enableReports();
  return "BNO08x";
}

static bool read(Reading &out) {
  // After a reset of its own the chip forgets the reports it was asked for.
  if (bno08x.wasReset()) enableReports();

  sh2_SensorValue_t value;
  while (bno08x.getSensorEvent(&value)) {
    if (value.sensorId == SH2_ACCELEROMETER) {
      accel[0] = value.un.accelerometer.x;
      accel[1] = value.un.accelerometer.y;
      accel[2] = value.un.accelerometer.z;
      haveAccel = true;
    } else if (value.sensorId == SH2_GYROSCOPE_CALIBRATED) {
      gyro[0] = value.un.gyroscope.x;
      gyro[1] = value.un.gyroscope.y;
      gyro[2] = value.un.gyroscope.z;
      accuracy = value.status & 0x03;
      haveGyro = true;
    }
  }
  if (!haveAccel || !haveGyro) return false;
  haveAccel = haveGyro = false;

  out.kind = Kind::Raw;
  for (int k = 0; k < 3; k++) {
    out.accel[k] = accel[k];
    out.gyro[k] = gyro[k];
  }
  return true;
}

// The gyro accuracy the chip reports, 0-3. It rises within seconds of
// moving; the boot screen does not wait for it (OK skips, as with the
// BNO055) - the shared filter does not need it to be 3.
static uint8_t calibrationLevel() { return 3; }

static void printCalibration(Print &out) {
  out.printf("Calibr.  : gyro accuracy %u/3 (the chip calibrates itself)\n", accuracy);
}

}  // namespace bno08x_chip

const Driver BNO08X = { bno08x_chip::detect, bno08x_chip::read, bno08x_chip::calibrationLevel, bno08x_chip::printCalibration };

}  // namespace imu

#endif  // IMU_WITH_BNO08X
