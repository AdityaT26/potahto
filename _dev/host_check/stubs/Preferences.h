#pragma once
// Functional in-memory NVS mock: keys are typed like real NVS so type
// mismatches behave like the real thing (get returns the default).
#include <Arduino.h>
#include <map>
#include <vector>
#include <string>
enum NvsT { NT_U8, NT_I8, NT_U16, NT_I16, NT_U32, NT_I32, NT_U64, NT_STR, NT_BLOB };
struct NvsVal { NvsT t; std::vector<uint8_t> d; };
extern std::map<std::string, std::map<std::string, NvsVal>> g_nvs;
extern int g_nvsWrites;
class Preferences {
  std::string ns; bool ro = false;
  std::map<std::string, NvsVal>& m() { return g_nvs[ns]; }
  template<class T> size_t putT(const char* k, NvsT t, T v) {
    if (ro || strlen(k) > 15) return 0;
    NvsVal x{t, std::vector<uint8_t>((uint8_t*)&v, (uint8_t*)&v + sizeof v)};
    m()[k] = x; g_nvsWrites++; return sizeof v; }
  template<class T> T getT(const char* k, NvsT t, T def) {
    auto it = m().find(k); if (it == m().end() || it->second.t != t) return def;
    T v; memcpy(&v, it->second.d.data(), sizeof v); return v; }
 public:
  bool begin(const char* n, bool readOnly = false, const char* = nullptr) { ns = n; ro = readOnly; if (strlen(n) > 15) return false; g_nvs[ns]; return true; }
  void end() {}
  bool clear() { m().clear(); return true; }
  bool remove(const char* k) { return m().erase(k) > 0; }
  bool isKey(const char* k) { return m().count(k) > 0; }
  size_t putUChar(const char* k, uint8_t v) { return putT(k, NT_U8, v); }
  size_t putChar(const char* k, int8_t v) { return putT(k, NT_I8, v); }
  size_t putUShort(const char* k, uint16_t v) { return putT(k, NT_U16, v); }
  size_t putShort(const char* k, int16_t v) { return putT(k, NT_I16, v); }
  size_t putUInt(const char* k, uint32_t v) { return putT(k, NT_U32, v); }
  size_t putInt(const char* k, int32_t v) { return putT(k, NT_I32, v); }
  size_t putULong64(const char* k, uint64_t v) { return putT(k, NT_U64, v); }
  size_t putBool(const char* k, bool v) { return putUChar(k, v ? 1 : 0); }
  size_t putFloat(const char* k, float v) { return putBytes(k, &v, 4); }
  size_t putString(const char* k, const char* v) { if (ro) return 0; m()[k] = NvsVal{NT_STR, std::vector<uint8_t>(v, v + strlen(v) + 1)}; g_nvsWrites++; return strlen(v); }
  size_t putString(const char* k, const String& v) { return putString(k, v.c_str()); }
  size_t putBytes(const char* k, const void* v, size_t n) { if (ro || strlen(k) > 15) return 0; m()[k] = NvsVal{NT_BLOB, std::vector<uint8_t>((uint8_t*)v, (uint8_t*)v + n)}; g_nvsWrites++; return n; }
  uint8_t getUChar(const char* k, uint8_t d = 0) { return getT(k, NT_U8, d); }
  int8_t getChar(const char* k, int8_t d = 0) { return getT(k, NT_I8, d); }
  uint16_t getUShort(const char* k, uint16_t d = 0) { return getT(k, NT_U16, d); }
  int16_t getShort(const char* k, int16_t d = 0) { return getT(k, NT_I16, d); }
  uint32_t getUInt(const char* k, uint32_t d = 0) { return getT(k, NT_U32, d); }
  int32_t getInt(const char* k, int32_t d = 0) { return getT(k, NT_I32, d); }
  uint64_t getULong64(const char* k, uint64_t d = 0) { return getT(k, NT_U64, d); }
  bool getBool(const char* k, bool d = false) { return getUChar(k, d ? 1 : 0) != 0; }
  float getFloat(const char* k, float d = 0) { float v = d; if (getBytesLength(k) == 4) getBytes(k, &v, 4); return v; }
  size_t getString(const char* k, char* out, size_t n) { auto it = m().find(k); if (it == m().end() || it->second.t != NT_STR) return 0; size_t l = std::min(n, it->second.d.size()); memcpy(out, it->second.d.data(), l); if (l) out[l-1] = 0; return l; }
  String getString(const char* k, const String& d = String()) { auto it = m().find(k); if (it == m().end() || it->second.t != NT_STR) return d; return String((const char*)it->second.d.data()); }
  size_t getBytesLength(const char* k) { auto it = m().find(k); return (it == m().end() || it->second.t != NT_BLOB) ? 0 : it->second.d.size(); }
  size_t getBytes(const char* k, void* out, size_t n) { auto it = m().find(k); if (it == m().end() || it->second.t != NT_BLOB || it->second.d.size() > n) return 0; memcpy(out, it->second.d.data(), it->second.d.size()); return it->second.d.size(); }
  size_t freeEntries() { return 400; }
};
