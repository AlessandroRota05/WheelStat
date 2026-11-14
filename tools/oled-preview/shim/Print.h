#pragma once

// Arduino's Print, with the same number formatting (printFloat rounds the
// same way), so the text on the images is the text on the panel.

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

class __FlashStringHelper;

#define DEC 10
#define HEX 16

class Print;

class Printable {
 public:
  virtual ~Printable() {}
  virtual size_t printTo(Print &p) const = 0;
};

class String {
 public:
  String(const char *s = "") { strncpy(buf_, s, sizeof(buf_) - 1); buf_[sizeof(buf_) - 1] = 0; }
  const char *c_str() const { return buf_; }
  unsigned length() const { return strlen(buf_); }
 private:
  char buf_[64];
};

class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t *buffer, size_t size) {
    size_t n = 0;
    while (size--) n += write(*buffer++);
    return n;
  }
  size_t write(const char *s) { return write((const uint8_t *)s, strlen(s)); }

  size_t print(const __FlashStringHelper *s) { return write((const char *)s); }
  size_t print(const String &s) { return write(s.c_str()); }
  size_t print(const char *s) { return write(s); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(unsigned char n, int base = DEC) { return printNumber(n, base); }
  size_t print(int n, int base = DEC) { return print((long)n, base); }
  size_t print(unsigned int n, int base = DEC) { return printNumber(n, base); }
  size_t print(long n, int base = DEC) {
    if (base == 10 && n < 0) return write('-') + printNumber((unsigned long)-n, 10);
    return printNumber((unsigned long)n, base);
  }
  size_t print(unsigned long n, int base = DEC) { return printNumber(n, base); }
  // size_t is 64 bits on a PC, 32 on the ESP32
  size_t print(unsigned long long n, int base = DEC) { return printNumber((unsigned long)n, base); }
  size_t print(double n, int digits = 2) { return printFloat(n, digits); }
  size_t print(const Printable &p) { return p.printTo(*this); }

  template <typename T> size_t println(T v) { return print(v) + write('\n'); }
  size_t println() { return write('\n'); }

  size_t printf(const char *format, ...) {
    char text[256];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return write(text);
  }

 private:
  size_t printNumber(unsigned long n, uint8_t base) {
    char buf[8 * sizeof(long) + 1];
    char *str = &buf[sizeof(buf) - 1];
    *str = '\0';
    if (base < 2) base = 10;
    do {
      char c = n % base;
      n /= base;
      *--str = c < 10 ? c + '0' : c + 'A' - 10;
    } while (n);
    return write(str);
  }

  size_t printFloat(double number, uint8_t digits) {
    size_t n = 0;
    if (number != number) return write("nan");
    if (number < 0.0) {
      n += write('-');
      number = -number;
    }
    double rounding = 0.5;
    for (uint8_t i = 0; i < digits; ++i) rounding /= 10.0;
    number += rounding;
    unsigned long intPart = (unsigned long)number;
    double remainder = number - (double)intPart;
    n += printNumber(intPart, 10);
    if (digits > 0) n += write('.');
    while (digits-- > 0) {
      remainder *= 10.0;
      unsigned int toPrint = (unsigned int)remainder;
      n += printNumber(toPrint, 10);
      remainder -= toPrint;
    }
    return n;
  }
};
