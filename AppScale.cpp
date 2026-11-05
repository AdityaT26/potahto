// VL53L0X distance gauge: units, tare, stored readings
#include "Multitool.h"

#define SCALE_MAX_STORE 10

static uint16_t s_store[SCALE_MAX_STORE];
static uint8_t  s_count = 0;
static bool     s_list  = false;
static int      s_lsel  = 0, s_ltop = 0;

static const char* unitLabel(uint8_t u) {
  return u == UNIT_MM ? "mm" : (u == UNIT_CM ? "cm" : "in");
}

// median-of-5 + exponential smoothing -> steady reading
static uint16_t scaleSmooth(uint16_t raw) {
  static uint16_t buf[5];
  static uint8_t  bi = 0, cnt = 0;
  static float    ema = -1;
  buf[bi] = raw; bi = (bi + 1) % 5; if (cnt < 5) cnt++;
  uint16_t t[5];
  for (int i = 0; i < cnt; i++) t[i] = buf[i];
  for (int i = 1; i < cnt; i++) {              // insertion sort
    uint16_t v = t[i]; int j = i - 1;
    while (j >= 0 && t[j] > v) { t[j + 1] = t[j]; j--; }
    t[j + 1] = v;
  }
  uint16_t med = t[cnt / 2];
  if (ema < 0) ema = med; else ema += (med - ema) * 0.25f;
  return (uint16_t)(ema + 0.5f);
}
static void fmtDist(int mm, uint8_t u, char* out, size_t n) {
  if (mm < 0) mm = 0;
  if (u == UNIT_MM)      snprintf(out, n, "%d", mm);
  else if (u == UNIT_CM) snprintf(out, n, "%.1f", mm / 10.0f);
  else                   snprintf(out, n, "%.2f", mm / 25.4f);
}

void scaleEnter() { s_list = false; }

static void loopList() {
  if (wantBack() || btnTap(BTN_LEFT)) { s_list = false; fbBack(); return; }
  if (s_count == 0) {
    uiHeader("Readings");
    u8g2.setFont(F_UI); uiCenter(38, "no readings");
    u8g2.setFont(F_TINY); uiCenter(60, "hold SEL = back");
    return;
  }
  if (btnRepeat(BTN_DOWN)) { if (s_lsel < s_count - 1) { s_lsel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_lsel > 0) { s_lsel--; fbNav(); } else fbEdge(); }
  int rows = uiListRows();
  if (s_lsel < s_ltop) s_ltop = s_lsel;
  if (s_lsel >= s_ltop + rows) s_ltop = s_lsel - rows + 1;

  if (btnTap(BTN_SELECT)) {                       // delete selected
    for (int i = s_lsel; i < s_count - 1; i++) s_store[i] = s_store[i + 1];
    s_count--;
    if (s_lsel >= s_count && s_lsel > 0) s_lsel--;
    fbConfirm(); toast("Deleted");
  }

  uiHeader("Readings");
  for (int r = 0; r < rows; r++) {
    int i = s_ltop + r;
    if (i >= s_count) break;
    char lab[16], val[16];
    snprintf(lab, sizeof(lab), "#%d", i + 1);
    char num[12]; fmtDist(s_store[i], S.scaleUnit, num, sizeof(num));
    snprintf(val, sizeof(val), "%s %s", num, unitLabel(S.scaleUnit));
    uiListRow(r, lab, val, i == s_lsel);
  }
  uiScrollbar(s_count, s_ltop, rows);
}

static void loopMain() {
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnRepeat(BTN_RIGHT)) { S.scaleUnit = (S.scaleUnit + 1) % UNIT_COUNT; fbNav(); settingsSaveSoon(); }
  if (btnRepeat(BTN_LEFT))  { S.scaleUnit = (S.scaleUnit + UNIT_COUNT - 1) % UNIT_COUNT; fbNav(); settingsSaveSoon(); }

  uint16_t raw = 0;
  bool ok = tofReadMm(raw);
  uint16_t mm = ok ? scaleSmooth(raw) : 0;         // stabilized reading

  if (btnTap(BTN_UP)) {                            // tare
    if (ok) { S.scaleTareMm = mm; toast("Tared"); fbConfirm(); settingsSaveSoon(); }
    else fbError();
  }
  if (btnTap(BTN_DOWN)) { s_list = true; s_lsel = 0; s_ltop = 0; fbSelect(); return; }
  if (btnTap(BTN_SELECT)) {                        // store
    if (ok && s_count < SCALE_MAX_STORE) { s_store[s_count++] = mm; toast("Saved"); fbConfirm(); }
    else fbError();
  }

  int shown = (int)mm - S.scaleTareMm;
  npxSetBase(NPX_SOLID, strip.Color(0, 60, 90), 4);

  uiHeader("Scale");

  // tare + count row
  u8g2.setFont(F_TINY);
  if (S.scaleTareMm) { char t[16]; snprintf(t, sizeof(t), "tare %d", S.scaleTareMm); u8g2.drawStr(2, 21, t); }
  char cnt[10]; snprintf(cnt, sizeof(cnt), "%d/%d", s_count, SCALE_MAX_STORE);
  uiRight(126, 21, cnt);

  // big value
  if (ok) {
    char num[12]; fmtDist(shown, S.scaleUnit, num, sizeof(num));
    uiBigValue(64, 42, num, unitLabel(S.scaleUnit));
  } else {
    u8g2.setFont(F_BIG); uiCenter(42, "---");
  }

  // range bar 0..2000mm
  uiProgressBar(14, 48, 100, 6, ok ? mm / 2000.0f : 0);

  u8g2.setFont(F_TINY);
  uiCenter(63, "LR unit  U tare  D list  SEL save");
}

void scaleLoop() {
  if (!tofPresent()) {
    uiHeader("Scale");
    u8g2.setFont(F_UI); uiCenter(38, "No ToF sensor");
    if (wantBack()) { fbBack(); goBack(); }
    return;
  }
  if (s_list) loopList(); else loopMain();
}
