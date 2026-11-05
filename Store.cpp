// LittleFS wrapper with atomic writes
#include "Multitool.h"
#include <LittleFS.h>

static bool s_ok = false;

bool storeBegin() {
  // formatOnFail: a fresh / never-used partition is formatted once.
  s_ok = LittleFS.begin(true, "/fs", 6, "spiffs");
  return s_ok;
}
bool storeOk() { return s_ok; }

bool storeRead(const char* path, String& out, size_t maxLen) {
  out = "";
  if (!s_ok || !LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  size_t n = f.size();
  if (n > maxLen) { f.close(); return false; }
  out.reserve(n + 1);
  char buf[256];
  while (f.available()) {
    int r = f.read((uint8_t*)buf, sizeof(buf));
    if (r <= 0) break;
    out.concat(buf, r);
  }
  f.close();
  return true;
}

bool storeWrite(const char* path, const char* data, size_t len) {
  if (!s_ok) return false;
  String tmp = String(path) + ".tmp";
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  size_t w = f.write((const uint8_t*)data, len);
  f.close();
  if (w != len) { LittleFS.remove(tmp); return false; }
  LittleFS.remove(path);
  return LittleFS.rename(tmp, path);
}

bool storeRemove(const char* path) { return s_ok && LittleFS.remove(path); }

size_t storeFreeBytes() { return s_ok ? LittleFS.totalBytes() - LittleFS.usedBytes() : 0; }
