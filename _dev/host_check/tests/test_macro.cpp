#include "Multitool.h"
#include "HidPriv.h"
#include <vector>
#include <string>
#include <fstream>
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
struct Rep { char kind; int a, b; uint32_t t; };
static std::vector<Rep> R;
static uint8_t btn = 0;
void hidxKbd(uint8_t m, uint8_t u) { R.push_back({'k', m, u, g_fakeMillis}); }
void hidxCons(uint16_t u) { R.push_back({'c', u, 0, g_fakeMillis}); }
uint8_t hidxButtons() { return btn; }
bool hidxIsUsb() { return true; }
bool hidReady() { return true; }
void hidMouseButtons(uint8_t b) { btn = b; R.push_back({'b', b, 0, g_fakeMillis}); }
void hidMouseMove(int, int, int w, int) { R.push_back({'w', w, 0, g_fakeMillis}); }
void hidReleaseAll() { R.push_back({'x', 0, 0, g_fakeMillis}); }
static std::string lastEv; static int lastPtr = -1;
void hidxPointer(uint8_t m) { lastPtr = m; R.push_back({'p', m, 0, g_fakeMillis}); }
void hidxEvent(const char* k, const char* a) { lastEv = std::string(k) + "|" + a; R.push_back({'e', 0, 0, g_fakeMillis}); }
static void run(const char* m) { R.clear(); char e[48]; CHECK(hidRunMacro(m, e, sizeof e)); for (int i = 0; i < 4000 && hidBusy(); i++) { g_fakeMillis += 1; hidMacroTick(); } CHECK(!hidBusy()); }
static std::string err(const char* m) { char e[48]; if (hidCheckMacro(m, e, sizeof e)) return "OK"; return e; }
static std::vector<std::pair<int,int>> keys() { std::vector<std::pair<int,int>> v; for (auto& r : R) if (r.kind == 'k') v.push_back({r.a, r.b}); return v; }
int main() {
  run("Hi!{enter}");
  auto k = keys();
  std::vector<std::pair<int,int>> want = {{2,0x0B},{0,0},{0,0x0C},{0,0},{2,0x1E},{0,0},{0,0x28},{0,0}};
  CHECK(k == want);
  run("{ctrl+shift+esc}"); CHECK(keys() == (std::vector<std::pair<int,int>>{{3,0x29},{0,0}}));
  run("{WIN+R}{delay 300}n");
  CHECK(R.size() == 4 && R[0].a == HM_LGUI && R[0].b == 0x15 && R[2].b == 0x11 && R[2].t - R[1].t >= 300);
  run("{{"); CHECK(keys()[0] == std::make_pair(2, 0x2F));
  run("{ctrl++}"); CHECK(keys()[0] == std::make_pair(1, 0x2E));
  run("{f13}{F24}{f1}"); CHECK(keys()[0].second == 0x68 && keys()[2].second == 0x73 && keys()[4].second == 0x3A);
  run("{media volup}"); CHECK(R.size() == 2 && R[0].kind == 'c' && R[0].a == 0xE9 && R[1].a == 0);
  run("{mouse right}"); CHECK(R.size() == 2 && R[0].kind == 'b' && R[0].a == HB_RIGHT && R[1].a == 0);
  run("{scroll -3}"); CHECK(R.size() == 1 && R[0].kind == 'w' && R[0].a == -3);
  run("h\xC3\xA9llo"); CHECK(keys().size() == 8);            // é skipped
  run("a\nb\t"); CHECK(keys()[2].second == 0x28 && keys()[6].second == 0x2B);
  run("{ctrl+`}{ctrl+/}{shift+alt+f}"); CHECK(keys()[0] == std::make_pair(1, 0x35) && keys()[2] == std::make_pair(1, 0x38) && keys()[4] == std::make_pair(6, 0x09));
  // 1.3 tokens
  run("{run Notepad}"); { auto v = keys(); CHECK(v[0] == std::make_pair((int)HM_LGUI, 0x15) && v[2] == std::make_pair(2, 0x11) && v[v.size() - 2].second == 0x28); }
  run("{cmd ipconfig}"); { auto v = keys(); CHECK(v[2] == std::make_pair(0, 0x06) && v[v.size() - 2] == std::make_pair(0, 0x28)); }       // "c"md ... Enter
  run("{admin ipconfig /flushdns}"); { auto v = keys(); CHECK(v[v.size() - 4] == std::make_pair(3, 0x28) && v[v.size() - 2] == std::make_pair((int)HM_LALT, 0x1C)); }
  run("{pc shutdown /h}{PC Code C:\\Work}"); CHECK(lastEv == "pc.run|Code C:\\Work" && R.size() == 2);
  run("{win+shift+s}{pointer drag}"); CHECK(lastPtr == PTR_DRAG && R.back().kind == 'p');
  run("{pointer}"); CHECK(lastPtr == PTR_FREE);
  CHECK(err("{pointer fly}") == "pointer mode must be click, drag or empty");
  CHECK(err("{run}") == "{run} needs something to run");
  CHECK(err("{ctrl+foo}") == "unknown key 'foo'");
  CHECK(err("{delay 0}") == "delay must be 1..30000 ms");
  CHECK(err("{media nope}") == "unknown media key 'nope'");
  CHECK(err("abc{enter") == "missing '}'");
  CHECK(err("{}") == "bad token");
  CHECK(err("{mouse side}") == "mouse button must be left/right/middle");
  CHECK(err(std::string(601, 'a').c_str()) == "macro too long (max 600)");
  CHECK(err(std::string(600, 'a').c_str()) == "OK");
  // a failed macro must not leave half a macro queued
  char e[48]; CHECK(!hidRunMacro("abc{nope}", e, sizeof e) && !hidBusy());
  // queue overflow rolls back cleanly
  std::string big(600, 'x'); CHECK(hidRunMacro(big.c_str(), e, sizeof e));
  CHECK(!hidRunMacro(big.c_str(), e, sizeof e) && std::string(e) == "macro queue full");
  hidCancel(); CHECK(!hidBusy());
  // every shipped default macro parses
  std::ifstream f("default_macros.txt"); std::string line; int n = 0;
  while (std::getline(f, line)) { std::string r = err(line.c_str()); if (r != "OK") { printf("default '%s': %s\n", line.c_str(), r.c_str()); exit(1); } n++; }
  printf("macro engine: all checks PASS (%d default macros valid)\n", n);
}
