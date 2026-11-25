/*
  WheelStat - tests of src/core, the part of the firmware that does not
  depend on the hardware. They run on a PC:

      pio test -e native

  Nothing here talks to a sensor: a ride is simulated by feeding the logic
  the position reports a GPS would send.
*/

#include <math.h>
#include <string.h>
#include <unity.h>

#include "src/core/Ahrs.h"
#include "src/core/AxisMap.h"
#include "src/core/Gate.h"
#include "src/core/GpsCommands.h"
#include "src/core/SensorMath.h"
#include "src/core/GpsClock.h"
#include "src/core/GripRisk.h"
#include "src/core/LapLogic.h"
#include "src/core/PeakFilter.h"
#include "src/core/SessionCodec.h"
#include "src/core/SettingsRules.h"
#include "src/core/Trace.h"

using namespace core;

// ===========================================================================
// Attitude filter (Ahrs) - simulated physics
// ===========================================================================
// Bike axes: x forward, y left, z up. A coordinated corner at constant
// speed v and lean phi (positive right) has yaw rate W = -g tan(phi) / v
// (negative = turning right). With R = Rz(psi) Rx(phi), the sensors read
//   specific force  f = Rx(-phi) (0, v W, g)
//   angular rate    w = (phi', W sin(phi), W cos(phi))
// which is what the accelerometer and gyro of a real bike would report.

static void cornerSensors(float phiRad, float phiRate, float v, float gyro[3], float accel[3]) {
  const float g = 9.81f;
  float W = v > 0 ? -g * tanf(phiRad) / v : 0;
  float c = cosf(phiRad), s = sinf(phiRad);
  accel[0] = 0;
  accel[1] = c * v * W + s * g;
  accel[2] = -s * v * W + c * g;
  gyro[0] = phiRate;
  gyro[1] = W * s;
  gyro[2] = W * c;
}

void test_ahrs_still_and_level() {
  core::Ahrs ahrs;
  float gyro[3] = {0, 0, 0}, accel[3] = {0, 0, 9.81f};
  core::Attitude a = {};
  for (int i = 0; i < 400; i++) a = ahrs.update(gyro, accel, 0.005f);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, a.leanDeg);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, a.pitchDeg);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, a.accelLong);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, a.accelLat);
}

// Upright, then into a right-hand corner: lean grows to `degrees` over two
// seconds and holds for ten, at 90 km/h. The filter must follow it.
static float rideCorner(bool knowSpeed, float degrees) {
  core::Ahrs ahrs;
  const float dt = 0.005f, v = 25.0f, target = degrees * 3.14159265f / 180;
  float gyro[3], accel[3], t = 0;
  core::Attitude a = {};
  for (int i = 0; i < 4000; i++, t += dt) {
    float phi = t < 2 ? target * t / 2 : target;
    float rate = t < 2 ? target / 2 : 0;
    cornerSensors(phi, rate, v, gyro, accel);
    ahrs.setSpeed(knowSpeed ? v : 0, t);
    a = ahrs.update(gyro, accel, dt);
  }
  return a.leanDeg;
}

void test_ahrs_steady_corner_reads_the_lean() {
  TEST_ASSERT_FLOAT_WITHIN(1.5f, 40.0f, rideCorner(true, 40));
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 20.0f, rideCorner(true, 20));
}

// The reason for the speed correction: without it, in a long corner the
// accelerometer sees the bike's own "up" (about 1 G at moderate lean, so it
// is believed) and pulls the lean back towards zero. Kept as a test so
// nobody removes the correction as dead weight.
void test_ahrs_steady_corner_without_speed_cannot_see_the_lean() {
  TEST_ASSERT_TRUE(fabsf(rideCorner(false, 20) - 20.0f) > 10.0f);
}

// Straight, upright, braking at 0.25 G for four seconds with the GPS
// speed arriving at 10 Hz: no stoppie must appear, and the deceleration
// must be read as such.
void test_ahrs_braking_is_not_a_stoppie() {
  core::Ahrs ahrs;
  const float dt = 0.005f, decel = -2.45f;
  float gyro[3] = {0, 0, 0}, accel[3] = {decel, 0, 9.81f};
  float worstPitch = 0, t = 0, v = 30.0f, gpsSpeed = v;
  core::Attitude a = {};
  for (int i = 0; i < 800; i++, t += dt) {   // first, upright at speed
    float still[3] = {0, 0, 9.81f};
    ahrs.setSpeed(v, 0);
    a = ahrs.update(gyro, still, dt);
  }
  for (int i = 0; i < 800; i++, t += dt) {
    v += decel * dt;
    if (i % 20 == 0) gpsSpeed = v;            // 10 Hz
    ahrs.setSpeed(gpsSpeed, t);
    a = ahrs.update(gyro, accel, dt);
    if (i > 100) worstPitch = fmaxf(worstPitch, fabsf(a.pitchDeg));
  }
  TEST_ASSERT_TRUE(worstPitch < 2.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.3f, decel, a.accelLong);
}

void test_axis_map_upside_down() {
  // Breakout flipped over, X still forward: Y and Z point the other way.
  core::AxisMap map = {1, -2, -3};
  float chip[3] = {1, 2, -9.81f}, bike[3];
  map.apply(chip, bike);
  TEST_ASSERT_EQUAL_FLOAT(1, bike[0]);
  TEST_ASSERT_EQUAL_FLOAT(-2, bike[1]);
  TEST_ASSERT_EQUAL_FLOAT(9.81f, bike[2]);
  TEST_ASSERT_TRUE(map.valid());
  core::AxisMap twice = {1, 1, 3};
  TEST_ASSERT_FALSE(twice.valid());
}

// ===========================================================================
// Sensors read without a library
// ===========================================================================

void test_crc8_sensirion_example() {
  const uint8_t word[2] = {0xBE, 0xEF};   // the datasheets' own example
  TEST_ASSERT_EQUAL_HEX8(0x92, core::crc8(word, 2));
}

void test_crc16_modbus_check_value() {
  const uint8_t text[] = "123456789";
  TEST_ASSERT_EQUAL_HEX16(0x4B37, core::crc16Modbus(text, 9));
}

void test_sensor_conversions() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, core::sensirionTemperature(26214));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, core::sht3xHumidity(32768));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 56.5f, core::sht4xHumidity(32768));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, core::sht4xHumidity(65535));  // clamped
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 25.0f, core::si7021Temperature(26797));
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 50.0f, core::si7021Humidity(29360));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, core::hdc1080Temperature(25817));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, core::hdc1080Humidity(32768));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, core::htu31Temperature(25817));
}

void test_aht_decode() {
  // 50 % = 0x80000, 25 degrees = 0x60000 (20 bits each, packed)
  const uint8_t frame[6] = {0x1C, 0x80, 0x00, 0x06, 0x00, 0x00};
  float t, h;
  TEST_ASSERT_TRUE(core::ahtDecode(frame, t, h));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, h);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, t);
  const uint8_t busy[6] = {0x9C, 0, 0, 0, 0, 0};
  TEST_ASSERT_FALSE(core::ahtDecode(busy, t, h));
}

void test_am2320_decode() {
  uint8_t frame[8] = {0x03, 0x04, 0x01, 0xF4, 0x80, 0x65, 0, 0};   // 50.0 %, -10.1 degrees
  uint16_t crc = core::crc16Modbus(frame, 6);
  frame[6] = crc & 0xFF;
  frame[7] = crc >> 8;
  float t, h;
  TEST_ASSERT_TRUE(core::am2320Decode(frame, t, h));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, h);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.1f, t);
  frame[3] ^= 1;  // a flipped bit must not pass
  TEST_ASSERT_FALSE(core::am2320Decode(frame, t, h));
}

void test_bme280_temperature_datasheet_example() {
  // Bosch's worked example: T1=27504, T2=26435, T3=-1000, adc_T=519888
  // -> t_fine = 128422, T = 25.08 degrees.
  core::Bme280Calibration c = {};
  c.T1 = 27504; c.T2 = 26435; c.T3 = -1000;
  int32_t tFine;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.08f, core::bme280Temperature(c, 519888, tFine));
  TEST_ASSERT_EQUAL_INT32(128422, tFine);
}

// ===========================================================================
// GPS commands
// ===========================================================================

void test_nmea_sentence_checksums() {
  char out[48];
  TEST_ASSERT_EQUAL(16, core::nmeaSentence("PCAS02,100", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("$PCAS02,100*1E\r\n", out);
  core::nmeaSentence("PAIR050,100", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("$PAIR050,100*22\r\n", out);
  TEST_ASSERT_EQUAL(0, core::nmeaSentence("PCAS02,100", out, 8));   // does not fit
}

void test_ubx_frame_cfg_rate() {
  // UBX-CFG-RATE, 200 ms, 1 cycle, GPS time: the classic 5 Hz command
  const uint8_t payload[6] = {0xC8, 0x00, 0x01, 0x00, 0x01, 0x00};
  const uint8_t expected[14] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8,
                                0x00, 0x01, 0x00, 0x01, 0x00, 0xDE, 0x6A};
  uint8_t frame[32];
  TEST_ASSERT_EQUAL(14, core::ubxFrame(0x06, 0x08, payload, 6, frame, sizeof(frame)));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, frame, 14);
}

void setUp() {}
void tearDown() {}

// ===========================================================================
// Helpers: a small world in metres around a point in northern Italy
// ===========================================================================

static const double ORIGIN_LAT = 45.0;
static const double ORIGIN_LON = 9.0;
static const float  PI_F = 3.14159265f;

static GpsSample reportAt(float east, float north, float speedKmh, float courseDeg,
                          uint32_t timeMs) {
  GpsSample sample;
  sample.lat = ORIGIN_LAT + north / METRES_PER_DEGREE;
  sample.lon = ORIGIN_LON + east / (METRES_PER_DEGREE * cos(ORIGIN_LAT * RADIANS_PER_DEGREE));
  sample.speedKmh  = speedKmh;
  sample.courseDeg = courseDeg;
  sample.timeMs    = timeMs;
  return sample;
}

static Gate gateAt(float east, float north, float headingDeg, uint8_t confidence) {
  GpsSample position = reportAt(east, north, 0, 0, 0);
  return { position.lat, position.lon, headingDeg, confidence };
}

// A circular circuit ridden clockwise at constant speed. Angle 0 is the
// origin, where the bike heads east; the centre is `radius` to the south.
struct Circuit {
  float radius;
  float speedMs;

  float lapSeconds() const { return 2.0f * PI_F * radius / speedMs; }

  GpsSample report(float seconds, float startAngle = -0.5f) const {
    float angle = startAngle + seconds * speedMs / radius;
    float course = fmodf(90.0f + angle * 180.0f / PI_F, 360.0f);
    if (course < 0) course += 360.0f;
    return reportAt(radius * sinf(angle), -radius + radius * cosf(angle), speedMs * 3.6f,
                    course, (uint32_t)lroundf(seconds * 1000.0f));
  }

  Gate gateAtAngle(float angle) const {
    return gateAt(radius * sinf(angle), -radius + radius * cosf(angle), 0, GATE_UNKNOWN);
  }
};

// Rides `seconds` of the circuit at 5 Hz and returns the highest event.
static LapEvent ride(LapLogic &laps, const Circuit &circuit, float fromSeconds, float seconds) {
  LapEvent highest = LapEvent::None;
  int reports = (int)(seconds * 5.0f);
  for (int i = 0; i <= reports; i++) {
    LapEvent event = laps.onFix(circuit.report(fromSeconds + i * 0.2f));
    if (event > highest) highest = event;
  }
  return highest;
}

// ===========================================================================
// Gate
// ===========================================================================

static void test_gate_forward_pass_reports_where_the_line_was_crossed() {
  Gate gate = gateAt(0, 0, 90, GATE_CONFIRMED);
  Crossing crossing = detectCrossing(gate, reportAt(-2, 0, 60, 90, 0), reportAt(8, 0, 60, 90, 200));
  TEST_ASSERT_TRUE(crossing.pass == Pass::Forward);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.2f, crossing.fraction);
}

static void test_gate_ignores_the_wrong_way_once_confirmed() {
  Gate gate = gateAt(0, 0, 90, GATE_CONFIRMED);
  Crossing crossing = detectCrossing(gate, reportAt(5, 0, 60, 270, 0), reportAt(-5, 0, 60, 270, 200));
  TEST_ASSERT_TRUE(crossing.pass == Pass::None);
}

static void test_gate_ignores_a_pass_outside_its_width() {
  Gate gate = gateAt(0, 0, 90, GATE_CONFIRMED);
  float beside = GATE_HALF_WIDTH_M + 5.0f;
  Crossing crossing =
    detectCrossing(gate, reportAt(-5, beside, 60, 90, 0), reportAt(5, beside, 60, 90, 200));
  TEST_ASSERT_TRUE(crossing.pass == Pass::None);
}

static void test_gate_ignores_gps_noise_on_a_parked_bike() {
  Gate gate = gateAt(0, 0, 90, GATE_CONFIRMED);
  Crossing crossing = detectCrossing(gate, reportAt(-1, 0, 0.8f, 90, 0), reportAt(1, 0, 0.8f, 90, 200));
  TEST_ASSERT_TRUE(crossing.pass == Pass::None);
}

static void test_gate_learns_from_the_first_pass() {
  Gate gate = gateAt(0, 0, 0, GATE_UNKNOWN);
  Crossing crossing = detectCrossing(gate, reportAt(0, 6, 40, 180, 0), reportAt(0, -2, 40, 180, 200));
  TEST_ASSERT_TRUE(crossing.pass == Pass::Learned);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.75f, crossing.fraction);
}

static void test_gate_learns_after_a_standing_start_on_the_line() {
  Gate gate = gateAt(0, 0, 0, GATE_UNKNOWN);

  // Still too slow a few metres out...
  Crossing slow = detectCrossing(gate, reportAt(1, 0, 6, 90, 0), reportAt(2, 0, 9, 90, 200));
  TEST_ASSERT_TRUE(slow.pass == Pass::None);

  // ...then fast enough, already past the line: it counts, from this report.
  Crossing fast = detectCrossing(gate, reportAt(3, 0, 12, 90, 400), reportAt(5, 0, 18, 90, 600));
  TEST_ASSERT_TRUE(fast.pass == Pass::Learned);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, fast.fraction);
}

static void test_gate_does_not_learn_far_past_the_point() {
  Gate gate = gateAt(0, 0, 0, GATE_UNKNOWN);
  float past = GATE_LEARN_RUNOUT_M + 10.0f;
  Crossing crossing =
    detectCrossing(gate, reportAt(past, 0, 50, 90, 0), reportAt(past + 3, 0, 50, 90, 200));
  TEST_ASSERT_TRUE(crossing.pass == Pass::None);
}

static void test_gate_seen_once_can_still_be_turned_around() {
  Gate gate = gateAt(0, 0, 90, GATE_PROVISIONAL);
  Crossing crossing = detectCrossing(gate, reportAt(4, 0, 40, 270, 0), reportAt(-4, 0, 40, 270, 200));
  TEST_ASSERT_TRUE(crossing.pass == Pass::Reversed);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, crossing.fraction);
}

// ===========================================================================
// Lap logic
// ===========================================================================

static Course courseOn(const Circuit &circuit, int checkpoints) {
  Course course = {};
  course.finishSet = true;
  course.finish = circuit.gateAtAngle(0);
  course.checkpointCount = (uint8_t)checkpoints;
  for (int i = 0; i < checkpoints; i++)
    course.checkpoints[i] = circuit.gateAtAngle((i + 1) * PI_F / 2.0f);
  return course;
}

static void test_laps_nothing_happens_without_a_course() {
  Circuit circuit = { 200, 20 };
  LapLogic laps;
  TEST_ASSERT_TRUE(ride(laps, circuit, 0, 120) == LapEvent::None);
  TEST_ASSERT_FALSE(laps.running());
}

static void test_laps_time_is_exact_between_reports() {
  Circuit circuit = { 200, 20 };
  LapLogic laps;
  laps.setCourse(courseOn(circuit, 0));

  float lap = circuit.lapSeconds();  // 62.83 s: the line never falls on a report
  TEST_ASSERT_TRUE(ride(laps, circuit, 0, 30) == LapEvent::Started);
  TEST_ASSERT_TRUE(laps.running());
  TEST_ASSERT_EQUAL(0, laps.lapCount());

  TEST_ASSERT_TRUE(ride(laps, circuit, 30.2f, lap) == LapEvent::BestLap);
  TEST_ASSERT_EQUAL(1, laps.lapCount());
  TEST_ASSERT_UINT32_WITHIN(30, (uint32_t)(lap * 1000), laps.lapMs(0));

  ride(laps, circuit, 30.4f + lap, lap);
  TEST_ASSERT_EQUAL(2, laps.lapCount());
  TEST_ASSERT_UINT32_WITHIN(30, (uint32_t)(lap * 1000), laps.lapMs(1));
}

static void test_laps_direction_is_learned_then_confirmed() {
  Circuit circuit = { 200, 20 };
  LapLogic laps;
  laps.setCourse(courseOn(circuit, 0));
  TEST_ASSERT_FALSE(laps.takeCourseChanged());

  ride(laps, circuit, 0, 30);
  TEST_ASSERT_TRUE(laps.takeCourseChanged());
  TEST_ASSERT_FALSE(laps.takeCourseChanged());  // reported once
  TEST_ASSERT_EQUAL(GATE_PROVISIONAL, laps.course().finish.confidence);
  TEST_ASSERT_FLOAT_WITHIN(4.0f, 90.0f, laps.course().finish.headingDeg);

  ride(laps, circuit, 30.2f, circuit.lapSeconds());
  TEST_ASSERT_TRUE(laps.takeCourseChanged());
  TEST_ASSERT_EQUAL(GATE_CONFIRMED, laps.course().finish.confidence);
}

static void test_laps_sectors_add_up_to_the_lap() {
  Circuit circuit = { 200, 20 };
  LapLogic laps;
  laps.setCourse(courseOn(circuit, 2));
  TEST_ASSERT_EQUAL(3, laps.sectorCount());

  ride(laps, circuit, 0, 30 + circuit.lapSeconds());
  TEST_ASSERT_EQUAL(1, laps.lapCount());

  uint32_t quarter = (uint32_t)(circuit.lapSeconds() * 250);
  TEST_ASSERT_UINT32_WITHIN(40, quarter, laps.sectorMs(0, 0));
  TEST_ASSERT_UINT32_WITHIN(40, quarter, laps.sectorMs(0, 1));
  TEST_ASSERT_UINT32_WITHIN(40, 2 * quarter, laps.sectorMs(0, 2));
  TEST_ASSERT_EQUAL_UINT32(laps.lapMs(0),
                           laps.sectorMs(0, 0) + laps.sectorMs(0, 1) + laps.sectorMs(0, 2));
  TEST_ASSERT_EQUAL_UINT32(laps.sectorMs(0, 0), laps.bestSectorMs(0));
}

static void test_laps_next_gate_follows_the_lap() {
  Circuit circuit = { 200, 20 };
  LapLogic laps;
  Course course = courseOn(circuit, 1);
  laps.setCourse(course);

  bool isFinish = false;
  laps.nextGate(isFinish);
  TEST_ASSERT_TRUE(isFinish);  // stopwatch not armed yet

  ride(laps, circuit, 0, 20);  // past the line, not yet at the checkpoint
  const Gate &next = laps.nextGate(isFinish);
  TEST_ASSERT_FALSE(isFinish);
  TEST_ASSERT_EQUAL_DOUBLE(course.checkpoints[0].lat, next.lat);
}

static void test_laps_a_pass_too_soon_is_ignored() {
  Circuit circuit = { 20, 20 };  // a 6.3 s "lap": shorter than MIN_SECTOR_MS
  LapLogic laps;
  laps.setCourse(courseOn(circuit, 0));

  // Armed at 0.5 s; the passes at 6.8 and 13.1 s are too soon, the one at
  // 19.3 s closes a "lap" of three turns.
  ride(laps, circuit, 0, 21);
  TEST_ASSERT_EQUAL(1, laps.lapCount());
  TEST_ASSERT_UINT32_WITHIN(60, (uint32_t)(3 * circuit.lapSeconds() * 1000), laps.lapMs(0));
}

static void test_laps_riding_to_the_start_the_wrong_way_is_not_a_lap() {
  LapLogic laps;
  Course course = {};
  course.finishSet = true;
  course.finish = gateAt(0, 0, 0, GATE_UNKNOWN);
  laps.setCourse(course);

  // Westwards over the line, on the way to the start...
  LapEvent first = LapEvent::None;
  for (int i = 0; i <= 20; i++) {
    LapEvent event = laps.onFix(reportAt(40.0f - 4.0f * i, 0, 72, 270, i * 200));
    if (event > first) first = event;
  }
  TEST_ASSERT_TRUE(first == LapEvent::Started);

  // ...then back eastwards, the real start: no lap, the stopwatch restarts.
  LapEvent second = LapEvent::None;
  for (int i = 0; i <= 20; i++) {
    LapEvent event = laps.onFix(reportAt(-40.0f + 4.0f * i, 0, 72, 90, 30000 + i * 200));
    if (event > second) second = event;
  }
  TEST_ASSERT_TRUE(second == LapEvent::Started);
  TEST_ASSERT_EQUAL(0, laps.lapCount());
  TEST_ASSERT_UINT32_WITHIN(5, 32000, laps.lapStartMs());
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, laps.course().finish.headingDeg);
}

static void test_laps_reset_keeps_the_course() {
  Circuit circuit = { 200, 20 };
  LapLogic laps;
  laps.setCourse(courseOn(circuit, 0));
  ride(laps, circuit, 0, 30 + circuit.lapSeconds());
  TEST_ASSERT_EQUAL(1, laps.lapCount());

  laps.reset();
  TEST_ASSERT_EQUAL(0, laps.lapCount());
  TEST_ASSERT_EQUAL(-1, laps.bestLap());
  TEST_ASSERT_FALSE(laps.running());
  TEST_ASSERT_TRUE(laps.course().finishSet);

  const TracePoint *points;
  TEST_ASSERT_EQUAL(0, laps.closedTrace(points));
}

static void test_laps_closed_trace_covers_the_whole_lap() {
  Circuit circuit = { 200, 20 };
  LapLogic laps;
  laps.setCourse(courseOn(circuit, 0));
  ride(laps, circuit, 0, 30 + circuit.lapSeconds());

  const TracePoint *points;
  int count = laps.closedTrace(points);
  TEST_ASSERT_GREATER_OR_EQUAL(MAX_TRACE_POINTS / 2, count);
  TEST_ASSERT_LESS_OR_EQUAL(MAX_TRACE_POINTS, count);
  TEST_ASSERT_LESS_THAN_UINT32(250, points[0].timeMs);
  TEST_ASSERT_GREATER_THAN_UINT32(laps.lapMs(0) - 1000, points[count - 1].timeMs);
}

// ===========================================================================
// Trace and delta
// ===========================================================================

static void test_trace_thins_itself_out() {
  TraceRecorder trace;
  for (int i = 0; i < 1000; i++) trace.add(45.0, 9.0 + i * 1e-5, i * 200);

  TEST_ASSERT_GREATER_OR_EQUAL(MAX_TRACE_POINTS / 2, trace.count());
  TEST_ASSERT_LESS_OR_EQUAL(MAX_TRACE_POINTS, trace.count());
  TEST_ASSERT_EQUAL_UINT32(0, trace.points()[0].timeMs);

  // Evenly spaced, and reaching the end of what was fed.
  uint32_t step = trace.points()[1].timeMs - trace.points()[0].timeMs;
  for (int i = 1; i < trace.count(); i++)
    TEST_ASSERT_EQUAL_UINT32(step, trace.points()[i].timeMs - trace.points()[i - 1].timeMs);
  TEST_ASSERT_GREATER_THAN_UINT32(999 * 200 - step, trace.points()[trace.count() - 1].timeMs);

  trace.clear();
  TEST_ASSERT_EQUAL(0, trace.count());
}

// A straight reference ridden east at 10 m/s: one point every 10 m, 1 s.
static int straightReference(TracePoint *points, int count) {
  for (int i = 0; i < count; i++) {
    GpsSample at = reportAt(i * 10.0f, 0, 0, 0, 0);
    points[i] = { toE7(at.lat), toE7(at.lon), (uint32_t)i * 1000 };
  }
  return count;
}

static void test_delta_behind_and_ahead() {
  TracePoint reference[100];
  int count = straightReference(reference, 100);
  GpsSample here = reportAt(505, 0, 0, 0, 0);  // the reference was here at 50.5 s

  DeltaTracker tracker;
  int32_t delta = 0;
  TEST_ASSERT_TRUE(tracker.compute(reference, count, here.lat, here.lon, 52000, delta));
  TEST_ASSERT_INT32_WITHIN(20, 1500, delta);

  tracker.restart();
  TEST_ASSERT_TRUE(tracker.compute(reference, count, here.lat, here.lon, 49000, delta));
  TEST_ASSERT_INT32_WITHIN(20, -1500, delta);
}

static void test_delta_needs_a_reference_and_to_be_near_it() {
  TracePoint reference[100];
  int count = straightReference(reference, 100);
  DeltaTracker tracker;
  int32_t delta = 0;

  GpsSample here = reportAt(300, 0, 0, 0, 0);
  TEST_ASSERT_FALSE(tracker.compute(reference, 0, here.lat, here.lon, 1000, delta));
  TEST_ASSERT_FALSE(tracker.compute(reference, 1, here.lat, here.lon, 1000, delta));

  GpsSample away = reportAt(300, DELTA_MAX_OFFSET_M + 20.0f, 0, 0, 0);
  TEST_ASSERT_FALSE(tracker.compute(reference, count, away.lat, away.lon, 30000, delta));
}

static void test_delta_is_not_fooled_by_a_track_that_comes_back() {
  // Out 500 m east, then back on a parallel line 5 m to the north: at any
  // point the return leg is as near as the outward one.
  TracePoint reference[102];
  for (int i = 0; i <= 50; i++) {
    GpsSample out = reportAt(i * 10.0f, 0, 0, 0, 0);
    reference[i] = { toE7(out.lat), toE7(out.lon), (uint32_t)i * 1000 };
    GpsSample back = reportAt(500.0f - i * 10.0f, 5, 0, 0, 0);
    reference[51 + i] = { toE7(back.lat), toE7(back.lon), (uint32_t)(51 + i) * 1000 };
  }

  DeltaTracker tracker;
  int32_t delta = 0;

  // Start of the lap, slightly towards the return line: still the outward leg.
  GpsSample early = reportAt(50, 3, 0, 0, 0);
  TEST_ASSERT_TRUE(tracker.compute(reference, 102, early.lat, early.lon, 5000, delta));
  TEST_ASSERT_INT32_WITHIN(50, 0, delta);

  // Followed all the way round, the same place is now the return leg.
  for (int i = 6; i <= 50; i++) {
    GpsSample out = reportAt(i * 10.0f, 0, 0, 0, 0);
    tracker.compute(reference, 102, out.lat, out.lon, i * 1000, delta);
  }
  for (int i = 0; i <= 45; i++) {
    GpsSample back = reportAt(500.0f - i * 10.0f, 5, 0, 0, 0);
    TEST_ASSERT_TRUE(tracker.compute(reference, 102, back.lat, back.lon, (51 + i) * 1000, delta));
  }
  TEST_ASSERT_INT32_WITHIN(50, 0, delta);
}

// ===========================================================================
// GPS clock
// ===========================================================================

static void test_clock_ticks_with_the_gps_not_with_the_parser() {
  GpsClock clock;
  // Three reports 200 ms apart for the GPS, parsed in one burst after the
  // main loop was held up.
  TEST_ASSERT_EQUAL_UINT32(5000, clock.onFix(1000, 5000));
  TEST_ASSERT_EQUAL_UINT32(5200, clock.onFix(1200, 5000));
  TEST_ASSERT_EQUAL_UINT32(5400, clock.onFix(1400, 5001));
  TEST_ASSERT_EQUAL_UINT32(5450, clock.now(5051));
}

static void test_clock_crosses_midnight() {
  GpsClock clock;
  const int32_t day = 24L * 60 * 60 * 1000;
  clock.onFix(day - 100, 1000);
  TEST_ASSERT_EQUAL_UINT32(1200, clock.onFix(100, 1200));
}

static void test_clock_falls_back_to_millis() {
  GpsClock clock;
  TEST_ASSERT_EQUAL_UINT32(700, clock.now(700));  // before any report

  clock.onFix(1000, 5000);
  TEST_ASSERT_EQUAL_UINT32(5200, clock.onFix(-1, 5200));       // no time in the report
  TEST_ASSERT_EQUAL_UINT32(5400, clock.onFix(-1, 5400));
  TEST_ASSERT_EQUAL_UINT32(5600, clock.onFix(40000000, 5600)); // first valid one again
  TEST_ASSERT_EQUAL_UINT32(5800, clock.onFix(9000000, 5800));  // a jump that cannot be true
}

// ===========================================================================
// Pothole filter, maxima, events
// ===========================================================================

static const float THRESHOLDS[EVENT_COUNT] = { 15.0f, 10.0f, 35.0f, 0.70f, 0.50f };

// Feeds one channel at a constant level for `ms`, one sample every 10 ms.
static uint32_t hold(PeakFilter &filter, uint32_t from, uint32_t ms, Channel channel, float level) {
  float values[CHANNEL_COUNT] = {};
  values[channel] = level;
  for (uint32_t t = from; t < from + ms; t += 10) filter.sample(t, values, THRESHOLDS);
  return from + ms;
}

static void test_filter_drops_a_pothole_and_keeps_a_real_manoeuvre() {
  PeakFilter filter;
  filter.reset(0);

  uint32_t t = hold(filter, 0, 300, CH_G_BRAKE, 0.1f);
  t = hold(filter, t, 50, CH_G_BRAKE, 2.5f);   // the pothole
  t = hold(filter, t, 400, CH_G_BRAKE, 0.1f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, filter.sessionMax()[CH_G_BRAKE]);
  TEST_ASSERT_EQUAL(0, filter.eventCount(EV_HARD_BRAKE));

  t = hold(filter, t, 600, CH_G_BRAKE, 0.9f);  // real braking
  hold(filter, t, 300, CH_G_BRAKE, 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.9f, filter.sessionMax()[CH_G_BRAKE]);
  TEST_ASSERT_EQUAL(1, filter.eventCount(EV_HARD_BRAKE));
}

static void test_filter_interval_maxima_clear_on_their_own() {
  PeakFilter filter;
  filter.reset(0);
  uint32_t t = hold(filter, 0, 500, CH_SPEED, 120.0f);
  TEST_ASSERT_EQUAL_FLOAT(120.0f, filter.intervalMax()[CH_SPEED]);

  filter.clearIntervalMax();
  hold(filter, t, 500, CH_SPEED, 80.0f);
  TEST_ASSERT_EQUAL_FLOAT(80.0f, filter.intervalMax()[CH_SPEED]);
  TEST_ASSERT_EQUAL_FLOAT(120.0f, filter.sessionMax()[CH_SPEED]);
}

static void test_filter_counts_a_manoeuvre_once() {
  PeakFilter filter;
  filter.reset(0);

  uint32_t t = hold(filter, 0, 1000, CH_WHEELIE, 20.0f);
  t = hold(filter, t, 500, CH_WHEELIE, 12.0f);  // hovering: above 70 % of the threshold
  t = hold(filter, t, 1000, CH_WHEELIE, 20.0f);
  TEST_ASSERT_EQUAL(1, filter.eventCount(EV_WHEELIE));

  t = hold(filter, t, 500, CH_WHEELIE, 5.0f);   // really back down
  hold(filter, t, 1000, CH_WHEELIE, 20.0f);
  TEST_ASSERT_EQUAL(2, filter.eventCount(EV_WHEELIE));
}

static void test_filter_lean_counts_on_either_side() {
  PeakFilter filter;
  filter.reset(0);
  uint32_t t = hold(filter, 0, 500, CH_LEAN_LEFT, 40.0f);
  t = hold(filter, t, 500, CH_LEAN_LEFT, 0.0f);
  hold(filter, t, 500, CH_LEAN_RIGHT, 40.0f);
  TEST_ASSERT_EQUAL(2, filter.eventCount(EV_LEAN));
}

// ===========================================================================
// Session log
// ===========================================================================

static SampleValues someValues() {
  SampleValues values = {};
  values.lean = -12.4f;  values.pitch = 3.2f;
  values.gLat = -0.31f;  values.gLong = 0.12f;
  values.speedKmh = 64.2f;
  values.temperature = 18.4f;  values.humidity = 62.0f;
  values.lat = 45.46411;  values.lon = 9.18854;
  values.hasFix = true;  values.lap = 1;
  return values;
}

static void test_log_sample_survives_the_round_trip() {
  SampleValues out = unpackSample(packSample(someValues()));
  TEST_ASSERT_FLOAT_WITHIN(0.05f, -12.4f, out.lean);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 3.2f, out.pitch);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, -0.31f, out.gLat);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.12f, out.gLong);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 64.2f, out.speedKmh);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 18.4f, out.temperature);
  TEST_ASSERT_EQUAL_FLOAT(62.0f, out.humidity);
  TEST_ASSERT_DOUBLE_WITHIN(1e-7, 45.46411, out.lat);
  TEST_ASSERT_DOUBLE_WITHIN(1e-7, 9.18854, out.lon);
  TEST_ASSERT_TRUE(out.hasFix);
  TEST_ASSERT_EQUAL(1, out.lap);
}

static void test_log_sample_saturates_instead_of_wrapping() {
  SampleValues values = someValues();
  values.lean = 99999.0f;
  values.speedKmh = -5.0f;
  values.gLat = NAN;
  values.humidity = 140.0f;
  values.lap = 900;

  SessionSample sample = packSample(values);
  TEST_ASSERT_EQUAL_INT16(32767, sample.lean);
  TEST_ASSERT_EQUAL_UINT16(0, sample.speed);
  TEST_ASSERT_EQUAL_INT16(0, sample.gLat);
  TEST_ASSERT_EQUAL_UINT8(100, sample.humidity);
  TEST_ASSERT_EQUAL_UINT8(255, sample.lap);
}

static void test_log_csv_row() {
  char line[CSV_LINE_SIZE];
  formatCsvHeader(line, sizeof(line));
  TEST_ASSERT_EQUAL_STRING(
    "Time_s,Lean,Pitch,G_Lat,G_Long,Speed_Kmh,Temp_C,Hum_%,Risk_%,Lat,Lon,Lap", line);

  // Sample 10 at 3 Hz is 3.33 s in; 62 % and 18.4 C give a grip risk of 15.
  int length = formatCsvRow(packSample(someValues()), 10, 3, line, sizeof(line));
  TEST_ASSERT_EQUAL_STRING(
    "3.33,-12.4,3.2,-0.31,0.12,64.2,18.4,62,15,45.4641100,9.1885400,1", line);
  TEST_ASSERT_EQUAL((int)strlen(line), length);
}

static void test_log_csv_line_always_fits() {
  SampleValues values = {};
  values.lean = values.pitch = values.gLat = values.gLong = -9999.0f;
  values.speedKmh = 9999.0f;
  values.temperature = -9999.0f;
  values.humidity = 100.0f;
  values.lat = -89.9999999;  values.lon = -179.9999999;
  values.lap = 255;

  char line[CSV_LINE_SIZE];
  int length = formatCsvRow(packSample(values), 4000000000u, 1, line, sizeof(line));
  TEST_ASSERT_LESS_THAN((int)CSV_LINE_SIZE, length);
}

static void test_log_sample_count() {
  SessionHeader header = {};
  header.headerSize = sizeof(SessionHeader);
  header.sampleSize = sizeof(SessionSample);
  header.footerSize = sizeof(SessionFooter);

  uint32_t body = 100 * sizeof(SessionSample);
  TEST_ASSERT_EQUAL_UINT32(
    100, sampleCountFor(sizeof(SessionHeader) + body + sizeof(SessionFooter), header, true));
  // Power lost mid-sample: the partial one is not counted.
  TEST_ASSERT_EQUAL_UINT32(100, sampleCountFor(sizeof(SessionHeader) + body + 7, header, false));
  TEST_ASSERT_EQUAL_UINT32(0, sampleCountFor(10, header, false));
}

static void test_log_signed_peak() {
  TEST_ASSERT_EQUAL_FLOAT(30.0f, signedPeak(30.0f, 12.0f));
  TEST_ASSERT_EQUAL_FLOAT(-41.0f, signedPeak(8.0f, 41.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, signedPeak(0.0f, 0.0f));
}

static void test_grip_risk() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, gripRisk(25.0f, 40.0f));     // warm and dry
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.7f, gripRisk(18.4f, 62.0f));
  TEST_ASSERT_EQUAL_FLOAT(100.0f, gripRisk(-5.0f, 95.0f));   // capped
}

// ===========================================================================
// Settings
// ===========================================================================

static void test_settings_defaults() {
  Settings settings = {};
  applyDefaults(settings);
  sanitize(settings, "WheelStat");

  TEST_ASSERT_EQUAL_STRING("WheelStat", settings.wifiName);
  TEST_ASSERT_EQUAL_STRING("", settings.wifiPassword);  // generated later, on the device
  TEST_ASSERT_EQUAL(LOG_RATE_DEFAULT_HZ, settings.logRateHz);
  TEST_ASSERT_EQUAL_FLOAT(35.0f, settings.eventThreshold[EV_LEAN]);
  TEST_ASSERT_EQUAL_FLOAT(0.70f, settings.eventThreshold[EV_HARD_BRAKE]);
  TEST_ASSERT_EQUAL_FLOAT(LEAN_MAX_IDEAL_DEG, settings.leanAlertDeg);
}

static void test_settings_are_forced_into_their_limits() {
  Settings settings = {};
  applyDefaults(settings);
  settings.logRateHz = 200;
  settings.eventThreshold[EV_WHEELIE] = -3.0f;
  settings.eventThreshold[EV_HARD_ACCEL] = 50.0f;
  settings.eventThreshold[EV_LEAN] = NAN;
  settings.leanAlertDeg = 0.0f;
  sanitize(settings, "WheelStat");

  TEST_ASSERT_EQUAL(LOG_RATE_MAX_HZ, settings.logRateHz);
  TEST_ASSERT_EQUAL_FLOAT(eventLimits(EV_WHEELIE).min, settings.eventThreshold[EV_WHEELIE]);
  TEST_ASSERT_EQUAL_FLOAT(eventLimits(EV_HARD_ACCEL).max, settings.eventThreshold[EV_HARD_ACCEL]);
  TEST_ASSERT_EQUAL_FLOAT(eventLimits(EV_LEAN).fallback, settings.eventThreshold[EV_LEAN]);
  TEST_ASSERT_EQUAL_FLOAT(leanAlertLimits().min, settings.leanAlertDeg);

  settings.logRateHz = 0;
  sanitize(settings, "WheelStat");
  TEST_ASSERT_EQUAL(LOG_RATE_MIN_HZ, settings.logRateHz);
}

static void test_settings_wifi_credentials() {
  TEST_ASSERT_TRUE(validWifiName("WheelStat"));
  TEST_ASSERT_TRUE(validWifiName("Moto di Ale"));
  TEST_ASSERT_FALSE(validWifiName(""));
  TEST_ASSERT_FALSE(validWifiName(" Moto"));
  TEST_ASSERT_FALSE(validWifiName("NomeTroppoLungo"));
  TEST_ASSERT_FALSE(validWifiName("Mot\xC3\xB2"));  // not ASCII: the OLED cannot show it

  TEST_ASSERT_TRUE(validWifiPassword("abcd2345"));
  TEST_ASSERT_FALSE(validWifiPassword("short7"));
  TEST_ASSERT_FALSE(validWifiPassword("with space1"));
  TEST_ASSERT_FALSE(validWifiPassword("thirteenchars"));

  Settings settings = {};
  applyDefaults(settings);
  strcpy(settings.wifiName, "Pista");
  strcpy(settings.wifiPassword, "short7");
  sanitize(settings, "WheelStat");
  TEST_ASSERT_EQUAL_STRING("Pista", settings.wifiName);   // a valid name is kept
  TEST_ASSERT_EQUAL_STRING("", settings.wifiPassword);    // an invalid password is dropped

  // Garbage from flash, with no terminator anywhere.
  memset(settings.wifiName, 'x', sizeof(settings.wifiName));
  memset(settings.wifiPassword, 0x07, sizeof(settings.wifiPassword));
  sanitize(settings, "WheelStat");
  TEST_ASSERT_EQUAL_STRING("xxxxxxxxxxxx", settings.wifiName);
  TEST_ASSERT_EQUAL_STRING("", settings.wifiPassword);
}

static uint32_t countingRandom() {
  static uint32_t value = 12345;
  value = value * 1664525u + 1013904223u;
  return value >> 8;
}

static void test_settings_generated_password_is_usable() {
  char password[WIFI_PASSWORD_MAX + 1];
  generatePassword(password, 10, countingRandom);

  TEST_ASSERT_EQUAL(10, (int)strlen(password));
  TEST_ASSERT_TRUE(validWifiPassword(password));
  TEST_ASSERT_NULL(strpbrk(password, "0O1lI"));

  char another[WIFI_PASSWORD_MAX + 1];
  generatePassword(another, 10, countingRandom);
  TEST_ASSERT_TRUE(strcmp(password, another) != 0);
}

// ===========================================================================

int main(int, char **) {
  UNITY_BEGIN();

  RUN_TEST(test_gate_forward_pass_reports_where_the_line_was_crossed);
  RUN_TEST(test_gate_ignores_the_wrong_way_once_confirmed);
  RUN_TEST(test_gate_ignores_a_pass_outside_its_width);
  RUN_TEST(test_gate_ignores_gps_noise_on_a_parked_bike);
  RUN_TEST(test_gate_learns_from_the_first_pass);
  RUN_TEST(test_gate_learns_after_a_standing_start_on_the_line);
  RUN_TEST(test_gate_does_not_learn_far_past_the_point);
  RUN_TEST(test_gate_seen_once_can_still_be_turned_around);

  RUN_TEST(test_laps_nothing_happens_without_a_course);
  RUN_TEST(test_laps_time_is_exact_between_reports);
  RUN_TEST(test_laps_direction_is_learned_then_confirmed);
  RUN_TEST(test_laps_sectors_add_up_to_the_lap);
  RUN_TEST(test_laps_next_gate_follows_the_lap);
  RUN_TEST(test_laps_a_pass_too_soon_is_ignored);
  RUN_TEST(test_laps_riding_to_the_start_the_wrong_way_is_not_a_lap);
  RUN_TEST(test_laps_reset_keeps_the_course);
  RUN_TEST(test_laps_closed_trace_covers_the_whole_lap);

  RUN_TEST(test_trace_thins_itself_out);
  RUN_TEST(test_delta_behind_and_ahead);
  RUN_TEST(test_delta_needs_a_reference_and_to_be_near_it);
  RUN_TEST(test_delta_is_not_fooled_by_a_track_that_comes_back);

  RUN_TEST(test_clock_ticks_with_the_gps_not_with_the_parser);
  RUN_TEST(test_clock_crosses_midnight);
  RUN_TEST(test_clock_falls_back_to_millis);

  RUN_TEST(test_filter_drops_a_pothole_and_keeps_a_real_manoeuvre);
  RUN_TEST(test_filter_interval_maxima_clear_on_their_own);
  RUN_TEST(test_filter_counts_a_manoeuvre_once);
  RUN_TEST(test_filter_lean_counts_on_either_side);

  RUN_TEST(test_log_sample_survives_the_round_trip);
  RUN_TEST(test_log_sample_saturates_instead_of_wrapping);
  RUN_TEST(test_log_csv_row);
  RUN_TEST(test_log_csv_line_always_fits);
  RUN_TEST(test_log_sample_count);
  RUN_TEST(test_log_signed_peak);
  RUN_TEST(test_grip_risk);

  RUN_TEST(test_settings_defaults);
  RUN_TEST(test_settings_are_forced_into_their_limits);
  RUN_TEST(test_settings_wifi_credentials);
  RUN_TEST(test_settings_generated_password_is_usable);

  RUN_TEST(test_ahrs_still_and_level);
  RUN_TEST(test_ahrs_steady_corner_reads_the_lean);
  RUN_TEST(test_ahrs_steady_corner_without_speed_cannot_see_the_lean);
  RUN_TEST(test_ahrs_braking_is_not_a_stoppie);
  RUN_TEST(test_axis_map_upside_down);

  RUN_TEST(test_crc8_sensirion_example);
  RUN_TEST(test_crc16_modbus_check_value);
  RUN_TEST(test_sensor_conversions);
  RUN_TEST(test_aht_decode);
  RUN_TEST(test_am2320_decode);
  RUN_TEST(test_bme280_temperature_datasheet_example);

  RUN_TEST(test_nmea_sentence_checksums);
  RUN_TEST(test_ubx_frame_cfg_rate);

  return UNITY_END();
}
