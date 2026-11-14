#include "SessionCodec.h"

#include <math.h>
#include <stdio.h>

#include "Geo.h"
#include "GripRisk.h"

namespace core {

// Rounds and saturates instead of wrapping: a wild reading must not come
// back as a plausible value of the opposite sign.
static int32_t scaled(float value, float scale, int32_t low, int32_t high) {
  if (!(value == value)) return 0;  // NaN
  float result = roundf(value * scale);
  if (result < (float)low)  return low;
  if (result > (float)high) return high;
  return (int32_t)result;
}

SessionSample packSample(const SampleValues &values) {
  SessionSample sample = {};
  sample.latE7 = toE7(values.lat);
  sample.lonE7 = toE7(values.lon);
  sample.lean  = (int16_t)scaled(values.lean,  10.0f,  -32767, 32767);
  sample.pitch = (int16_t)scaled(values.pitch, 10.0f,  -32767, 32767);
  sample.gLat  = (int16_t)scaled(values.gLat,  100.0f, -32767, 32767);
  sample.gLong = (int16_t)scaled(values.gLong, 100.0f, -32767, 32767);
  sample.speed = (uint16_t)scaled(values.speedKmh, 10.0f, 0, 65535);
  sample.temperature = (int16_t)scaled(values.temperature, 10.0f, -32767, 32767);
  sample.humidity    = (uint8_t)scaled(values.humidity, 1.0f, 0, 100);
  sample.lap   = (uint8_t)(values.lap < 0 ? 0 : (values.lap > 255 ? 255 : values.lap));
  sample.flags = values.hasFix ? SAMPLE_HAS_FIX : 0;
  return sample;
}

SampleValues unpackSample(const SessionSample &sample) {
  SampleValues values = {};
  values.lat   = fromE7(sample.latE7);
  values.lon   = fromE7(sample.lonE7);
  values.lean  = sample.lean  / 10.0f;
  values.pitch = sample.pitch / 10.0f;
  values.gLat  = sample.gLat  / 100.0f;
  values.gLong = sample.gLong / 100.0f;
  values.speedKmh    = sample.speed / 10.0f;
  values.temperature = sample.temperature / 10.0f;
  values.humidity    = sample.humidity;
  values.lap    = sample.lap;
  values.hasFix = (sample.flags & SAMPLE_HAS_FIX) != 0;
  return values;
}

int formatCsvHeader(char *out, size_t size) {
  return snprintf(out, size,
                  "Time_s,Lean,Pitch,G_Lat,G_Long,Speed_Kmh,Temp_C,Hum_%%,Risk_%%,Lat,Lon,Lap");
}

int formatCsvRow(const SessionSample &sample, uint32_t index, uint8_t rateHz, char *out,
                 size_t size) {
  const SampleValues v = unpackSample(sample);
  const double seconds = rateHz > 0 ? (double)index / rateHz : 0.0;

  return snprintf(out, size, "%.2f,%.1f,%.1f,%.2f,%.2f,%.1f,%.1f,%.0f,%.0f,%.7f,%.7f,%d",
                  seconds, v.lean, v.pitch, v.gLat, v.gLong, v.speedKmh, v.temperature,
                  v.humidity, gripRisk(v.temperature, v.humidity), v.lat, v.lon, v.lap);
}

uint32_t sampleCountFor(uint32_t fileSize, const SessionHeader &header, bool hasFooter) {
  uint32_t overhead = header.headerSize + (hasFooter ? header.footerSize : 0);
  if (header.sampleSize == 0 || fileSize < overhead) return 0;
  return (fileSize - overhead) / header.sampleSize;
}

}  // namespace core
