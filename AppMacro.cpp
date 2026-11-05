// Macro Pad over USB or Bluetooth
//
// Profile list : UP/DOWN pick, SELECT open, hold SELECT back.
// Pad          : tap a direction / SELECT  -> its macro
//   "hold" layout    : hold a direction -> its second macro (9 per profile)
//   "folders" layout : hold a direction -> opens that folder; tap one of its
//                      five macros (25 per profile). Folders close after a
//                      macro, on hold SELECT, or after 4 s.
//   hold SELECT      : back to the profile list
// With the PC companion running and "Follow the PC app" on, the pad switches
// to the profile made for whatever app you are in (VS Code, Chrome, ...).
// Macros ending in {pointer ...} hand over to the gyro pointer (e.g. Snip).
// Edit profiles and grab more from the library in the web app (Macros page).
#include "Multitool.h"

static bool         s_inPad = false;
static int          s_sel = 0, s_top = 0;
static int          s_loaded = -1;
static MacroProfile s_prof;
static int8_t       s_flashKey = -1;
static uint32_t     s_flashUntil = 0;
static int8_t       s_folder = -1;           // open folder 0..3
static uint32_t     s_folderAt = 0;
static bool         s_skipTap[BTN_COUNT];    // the release that opened a folder

static bool loadSel(int i) {
  if (!macroLoadProfile(i, s_prof)) return false;
  s_loaded = i; S.macroProfile = (uint8_t)i; settingsSaveSoon();
  s_folder = -1;
  return true;
}

// called by Macros.cpp when the companion reports a new foreground app
void macroAppChanged(const char* exe) {
  if (!S.macroAuto || currentApp() != APP_MACRO) return;
  int p = macroProfileForApp(exe);
  if (p < 0 || p == s_loaded) return;
  s_sel = p;
  if (s_inPad && loadSel(p)) { char t[24]; snprintf(t, sizeof(t), "> %s", s_prof.name); toast(t); fbNav(); }
}

void macroEnter() {
  hidBleStart();
  s_inPad = false; s_folder = -1;
  s_sel = clampv<int>(S.macroProfile, 0, max(0, macroProfileCount() - 1));
  if (S.macroAuto && macroPcAppAge() < 600000UL) {           // companion knows the app: go straight in
    int p = macroProfileForApp(macroPcApp());
    if (p >= 0 && loadSel(p)) { s_sel = p; s_inPad = true; }
  }
}
void macroExit() { hidCancel(); pointerCancel(); }

static void fire(int k) {
  const MacroKey& mk = s_prof.keys[k];
  if (!mk.macro.length()) { fbEdge(); return; }
  char err[48];
  if (!hidRunMacro(mk.macro.c_str(), err, sizeof(err))) { toast(err); fbError(); return; }
  s_flashKey = k; s_flashUntil = millis() + 180;
  fbNav();
  npxFlash(strip.Color(0, 120, 200), 1, 120);
}

// one label cell; highlighted when it just fired
static void cell(int cx, int y, int w, int k, bool folderMark) {
  const char* t = s_prof.keys[k].label[0] ? s_prof.keys[k].label : "-";
  bool hi = (s_flashKey == k) && millis() < s_flashUntil;
  char buf[14]; u8g2.setFont(F_SMALL); uiTruncate(buf, sizeof(buf), t, w - (folderMark ? 8 : 4));
  int tw = u8g2.getStrWidth(buf) + (folderMark ? 4 : 0);
  if (hi) { u8g2.drawRBox(cx - w / 2, y - 8, w, 11, 2); u8g2.setDrawColor(0); }
  else u8g2.drawRFrame(cx - w / 2, y - 8, w, 11, 2);
  u8g2.drawStr(cx - tw / 2, y, buf);
  if (folderMark) { int fx = cx - tw / 2 + tw - 3; u8g2.drawFrame(fx, y - 5, 3, 4); u8g2.drawHLine(fx, y - 6, 2); }
  u8g2.setDrawColor(1);
}

static void drawCross(int base, bool center, bool folderMarks) {
  cell(64, 23, 54, base + 0, folderMarks);                // up
  cell(22, 38, 42, base + 2, folderMarks);                // left
  if (center) cell(64, 38, 40, base + 4, false);          // select
  cell(106, 38, 42, base + 3, folderMarks);               // right
  cell(64, 53, 54, base + 1, folderMarks);                // down
}

static void padLoop() {
  static const uint8_t DIRS[4] = { BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT };
  bool folders = s_prof.layout == ML_FOLDERS;
  uint32_t now = millis();

  if (s_folder >= 0) {                                    // ---- inside a folder ----
    if (wantBack() || now - s_folderAt > 4000) { s_folder = -1; fbBack(); return; }
    int base = MACRO_FOLDER0 + s_folder * 5;
    for (int d = 0; d < 4; d++) {
      if (btnReleased(DIRS[d]) && s_skipTap[DIRS[d]]) { s_skipTap[DIRS[d]] = false; continue; }
      if (btnTap(DIRS[d])) { fire(base + d); s_folder = -1; return; }
    }
    if (btnTap(BTN_SELECT)) { fire(base + 4); s_folder = -1; return; }
    uiHeader(s_prof.keys[MACRO_HOLD0 + s_folder].label[0] ? s_prof.keys[MACRO_HOLD0 + s_folder].label : "Folder");
    drawCross(base, true, false);
    u8g2.drawBox(0, 62, (int)(128 * (1.0f - (now - s_folderAt) / 4000.0f)), 2);   // closes in...
    return;
  }

  if (wantBack()) { s_inPad = false; hidCancel(); fbBack(); return; }
  for (int d = 0; d < 4; d++) {
    if (btnTap(DIRS[d])) fire(d);
    if (folders) {
      if (btnDown(DIRS[d]) && btnHeldMs(DIRS[d]) > 320) {
        s_folder = d; s_folderAt = now; s_skipTap[DIRS[d]] = true; fbSelect(); return;
      }
    } else if (btnLong(DIRS[d])) fire(MACRO_HOLD0 + d);
  }
  if (btnTap(BTN_SELECT)) fire(4);

  // "hold" layout: preview the hold layer while a direction is held
  bool holdLayer = false;
  if (!folders) for (int d = 0; d < 4; d++) if (btnHeldMs(DIRS[d]) > 200) holdLayer = true;

  uiHeader(s_prof.name);
  if (holdLayer) { drawCross(MACRO_HOLD0 - 0, false, false); u8g2.setFont(F_TINY); uiCenter(37, "HOLD"); }
  else drawCross(0, true, false);
  u8g2.setFont(F_TINY);
  // footer: where keys go + what holding does
  const char* rt = hidReady() ? (hidRoute() == HR_USB ? "USB" : "BT") : "no PC";
  u8g2.drawStr(0, 63, rt);
  if (folders) {
    // folder names in the corners
    char f[40]; snprintf(f, sizeof(f), "hold: %s/%s/%s/%s", s_prof.keys[5].label, s_prof.keys[6].label, s_prof.keys[7].label, s_prof.keys[8].label);
    uiTextClipped(18, 63, f, 108);
  } else uiRight(127, 63, hidBusy() ? "typing..." : "hold = 2nd");
  npxSetBase(NPX_SOLID, hidReady() ? strip.Color(0, 90, 160) : strip.Color(90, 60, 0), 3);
}

static void listLoop() {
  int n = macroProfileCount();
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnRepeat(BTN_DOWN)) { if (s_sel < n - 1) { s_sel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_sel > 0) { s_sel--; fbNav(); } else fbEdge(); }
  if ((btnTap(BTN_SELECT) || btnTap(BTN_RIGHT)) && n > 0) {
    if (loadSel(s_sel)) { s_inPad = true; fbSelect(); } else fbError();
    return;
  }
  int rows = uiListRows();
  if (s_sel < s_top) s_top = s_sel;
  if (s_sel >= s_top + rows) s_top = s_sel - rows + 1;
  uiHeader("Macro Pad");
  if (n == 0) { u8g2.setFont(F_SMALL); uiCenter(36, "No profiles"); return; }
  for (int r = 0; r < rows; r++) {
    int i = s_top + r; if (i >= n) break;
    uiListRow(r, macroProfileName(i), i == (int)S.macroProfile ? "*" : "", i == s_sel);
  }
  uiScrollbar(n, s_top, rows);
  npxSetBase(NPX_BREATHE, strip.Color(0, 60, 140), 3);
}

void macroLoop() { if (s_inPad) padLoop(); else listLoop(); }
