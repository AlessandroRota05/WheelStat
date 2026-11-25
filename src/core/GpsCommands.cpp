#include "GpsCommands.h"

#include <stdio.h>
#include <string.h>

namespace core {

size_t nmeaSentence(const char *body, char *out, size_t size) {
  uint8_t sum = 0;
  for (const char *c = body; *c; c++) sum ^= (uint8_t)*c;
  int n = snprintf(out, size, "$%s*%02X\r\n", body, sum);
  return (n > 0 && (size_t)n < size) ? (size_t)n : 0;
}

size_t ubxFrame(uint8_t msgClass, uint8_t msgId, const uint8_t *payload, uint16_t length,
                uint8_t *out, size_t size) {
  size_t total = 8 + (size_t)length;
  if (total > size) return 0;
  out[0] = 0xB5;
  out[1] = 0x62;
  out[2] = msgClass;
  out[3] = msgId;
  out[4] = (uint8_t)(length & 0xFF);
  out[5] = (uint8_t)(length >> 8);
  if (length) memcpy(out + 6, payload, length);
  uint8_t a = 0, b = 0;
  for (size_t i = 2; i < 6 + (size_t)length; i++) { a += out[i]; b += a; }
  out[6 + length] = a;
  out[7 + length] = b;
  return total;
}

}  // namespace core
