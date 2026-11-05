#pragma once
// Host-side stub of the Arduino/ESP32 core: just enough to compile + unit test.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <stdarg.h>
#include <ctype.h>
typedef bool boolean;
typedef uint8_t byte;
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define PI 3.14159265358979f
#define ADC_11db 3
#define ESP_ARDUINO_VERSION_MAJOR 3
#define IRAM_ATTR
#define PROGMEM
// never touch the real clock of the test machine
#define settimeofday(tv, tz) potahto_fake_settimeofday(tv, tz)
#define PGM_P const char*
#define strlcpy_fallback 1
extern uint32_t g_fakeMillis;
inline uint32_t millis() { return g_fakeMillis; }
inline uint32_t micros() { return g_fakeMillis * 1000; }
inline void delay(uint32_t ms) { g_fakeMillis += ms; }
inline void yield() {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
extern uint8_t g_pinLevel[64];
inline int  digitalRead(int p) { return (p >= 0 && p < 64) ? g_pinLevel[p] : HIGH; }
inline int  analogRead(int) { return 0; }
extern uint32_t g_fakeAdcMv;
inline uint32_t analogReadMilliVolts(int) { return g_fakeAdcMv; }
inline void analogReadResolution(int) {}
inline void analogSetPinAttenuation(int, int) {}
inline long map(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }
template<class T, class U> inline auto min(T a, U b) -> decltype(a < b ? a : b) { return a < b ? a : b; }
template<class T, class U> inline auto max(T a, U b) -> decltype(a > b ? a : b) { return a > b ? a : b; }
inline long random(long a) { return a ? rand() % a : 0; }
inline long random(long a, long b) { return a + random(b - a); }
inline void randomSeed(unsigned long s) { srand(s); }
inline uint32_t esp_random() { return (uint32_t)rand(); }
// LEDC (core 3.x)
inline bool ledcAttach(int, uint32_t, uint8_t) { return true; }
inline bool ledcDetach(int) { return true; }
inline bool setCpuFrequencyMhz(uint32_t) { return true; }
#ifndef RTC_DATA_ATTR
#define RTC_DATA_ATTR
#endif
inline bool ledcWrite(int, uint32_t) { return true; }
inline uint32_t ledcChangeFrequency(int, uint32_t f, uint8_t) { return f; }
inline uint32_t ledcWriteTone(int, uint32_t f) { return f; }
class String {
 public:
  std::string s;
  String() {}
  String(const char* c) : s(c ? c : "") {}
  String(const std::string& x) : s(x) {}
  String(int v) : s(std::to_string(v)) {}
  String(unsigned v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  String(unsigned long v) : s(std::to_string(v)) {}
  String(float v, int d = 2) { char b[32]; snprintf(b, 32, "%.*f", d, v); s = b; }
  const char* c_str() const { return s.c_str(); }
  size_t length() const { return s.size(); }
  String& operator+=(const String& o) { s += o.s; return *this; }
  bool concat(const char* c) { s += c; return true; }
  bool concat(const char* c, size_t n) { s.append(c, n); return true; }
  bool concat(char c) { s += c; return true; }
  bool concat(const String& o) { s += o.s; return true; }
  void replace(const char* a, const char* b) { size_t p = 0, la = strlen(a), lb = strlen(b); while ((p = s.find(a, p)) != std::string::npos) { s.replace(p, la, b); p += lb; } }
  bool operator==(const String& o) const { return s == o.s; }
  bool operator!=(const char* o) const { return s != o; }
  unsigned char reserve(unsigned int n) { s.reserve(n); return 1; }
  String& operator+=(const char* o) { s += o; return *this; }
  String& operator+=(char c) { s += c; return *this; }
  String operator+(const String& o) const { return String(s + o.s); }
  String operator+(const char* o) const { return String(s + o); }
  bool operator==(const char* o) const { return s == o; }
  char operator[](size_t i) const { return s[i]; }
  int toInt() const { return atoi(s.c_str()); }
  float toFloat() const { return atof(s.c_str()); }
  void trim() { while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back(); size_t i=0; while (i<s.size() && isspace((unsigned char)s[i])) i++; s.erase(0,i); }
  int indexOf(char c, int from = 0) const { auto p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  String substring(int a, int b = -1) const { return String(s.substr(a, b < 0 ? std::string::npos : b - a)); }
  bool startsWith(const char* p) const { return s.rfind(p, 0) == 0; }
};
#include "Esp.h"
class Print {
 public:
  virtual size_t write(uint8_t) { return 1; }
  virtual size_t write(const uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) write(b[i]); return n; }
  size_t print(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  size_t print(const String& s) { return print(s.c_str()); }
  size_t print(int v) { char b[16]; snprintf(b, 16, "%d", v); return print(b); }
  size_t print(unsigned v) { char b[16]; snprintf(b, 16, "%u", v); return print(b); }
  size_t print(long v) { char b[24]; snprintf(b, 24, "%ld", v); return print(b); }
  size_t print(unsigned long v) { char b[24]; snprintf(b, 24, "%lu", v); return print(b); }
  size_t print(float v, int d = 2) { char b[32]; snprintf(b, 32, "%.*f", d, v); return print(b); }
  size_t print(char c) { return write((uint8_t)c); }
  template<class T> size_t println(T v) { size_t n = print(v); return n + print("\n"); }
  size_t println() { return print("\n"); }
  size_t printf(const char* f, ...) __attribute__((format(printf, 2, 3)));
};
inline size_t Print::printf(const char* f, ...) { char b[256]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); return print(b); }
class Stream : public Print {
 public:
  virtual int available() { return 0; }
  virtual int read() { return -1; }
  virtual int peek() { return -1; }
};
class HWCDC : public Stream {
 public:
  void begin(unsigned long) {}
  void setRxBufferSize(size_t) {}
  void flush() {}
  void setTxTimeoutMs(uint32_t) {}
  operator bool() const { return true; }
  size_t write(uint8_t c) override { return 1; }
  using Print::write;
  int availableForWrite() { return 256; }
};
extern HWCDC Serial;
#include <stdarg.h>
#include <ctype.h>
