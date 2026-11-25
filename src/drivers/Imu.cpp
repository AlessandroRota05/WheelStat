#include "Imu.h"

#include "../../Config.h"
#include "imu/ImuDrivers.h"

namespace imu {

// Detection order. ICM-20948 and the MPU family share 0x68/0x69: the
// ICM-20948 has its identity in register 0, the MPUs in 0x75, and the
// ICM-20948 is asked first because an MPU's register 0 is not an ID.
static const Driver *const CANDIDATES[] = {
#ifdef IMU_WITH_BNO055
  &BNO055,
#endif
#ifdef IMU_WITH_BNO08X
  &BNO08X,
#endif
  &ICM20948, &MPU6050, &LSM6DS,
};

static const Driver *chosen = nullptr;
static const char   *chosenName = "-";

bool begin() {
  for (const Driver *driver : CANDIDATES) {
    const char *found = driver->detect();
    if (found != nullptr) {
      chosen = driver;
      chosenName = found;
      return true;
    }
  }
  return false;
}

bool read(Reading &out) { return chosen != nullptr && chosen->read(out); }

uint8_t calibrationLevel() { return chosen != nullptr ? chosen->calibrationLevel() : 0; }

void printCalibration(Print &out) {
  if (chosen != nullptr) chosen->printCalibration(out);
}

const char *name() { return chosenName; }

// ---------------------------------------------------------------------------
// Shared by the chips without fusion
// ---------------------------------------------------------------------------

void measureGyroBias(bool (*readRawGyro)(float gyro[3]), float bias[3]) {
  const int SAMPLES = 200;
  float sum[3] = {0, 0, 0};
  int taken = 0;
  for (int i = 0; i < SAMPLES; i++) {
    float g[3];
    if (readRawGyro(g)) {
      for (int k = 0; k < 3; k++) sum[k] += g[k];
      taken++;
    }
    delay(5);
  }
  for (int k = 0; k < 3; k++) bias[k] = taken ? sum[k] / taken : 0;
}

uint8_t readyLevel() { return 3; }

void printGyroBias(Print &out, const float bias[3]) {
  const float TO_DEG = 57.2957795f;
  out.printf("Gyro bias: %.2f %.2f %.2f deg/s (measured at boot)\n",
             bias[0] * TO_DEG, bias[1] * TO_DEG, bias[2] * TO_DEG);
}

}  // namespace imu
