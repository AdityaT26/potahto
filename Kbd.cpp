// On-screen keyboard and the unlock-code entry screen
#include "Multitool.h"

static const char* const PAGE[4] = {
  "abcdefghijklmnopqrstuvwxyz.-_@",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-_@",
  "1234567890!#$%&*()+=?/:;,'\"<>~",
  "[]{}\\|`^ .,-_@!?#$%&*()+=/:;'\"",
};
static const char* const PAGE_NAME[4] = { "abc", "ABC", "123", "#+=" };
enum { K_PAGE = 0, K_SPACE, K_DEL, K_OK };

static bool   s_on = false;
static char   s_title[24];
static char   s_txt[160];
static size_t s_cap = 0;
static char*  s_dst = nullptr;
static bool   s_mask = false;
static void (*s_done)(bool) = nullptr;
static int    s_page = 0, s_r = 0, s_c = 0;
static uint32_t s_typedAt = 0;

void kbdOpen(const char* title, char* buf, size_t cap, bool masked, void (*done)(bool)) {
  s_on = true; s_dst = buf; s_cap = min(cap, sizeof(s_txt)); s_mask = masked; s_done = done;
  strlcpy(s_title, title, sizeof(s_title));
  strlcpy(s_txt, buf ? buf : "", s_cap);
  s_page = 0; s_r = 0; s_c = 0;
  inputClearFrame();
}
bool kbdActive() { return s_on; }
static void finish(bool ok) {
  s_on = false;
  if (ok && s_dst) strlcpy(s_dst, s_txt, s_cap);
  void (*cb)(bool) = s_done; s_done = nullptr;
  inputClearFrame();
  if (cb) cb(ok);
}

void kbdUpdate() {
  if (wantBack()) { fbBack(); finish(false); return; }
  int cols = s_r == 3 ? 4 : 10;
  if (btnRepeat(BTN_RIGHT)) { s_c = (s_c + 1) % cols; fbNav(); }
  if (btnRepeat(BTN_LEFT))  { s_c = (s_c + cols - 1) % cols; fbNav(); }
  if (btnRepeat(BTN_DOWN))  { int nr = (s_r + 1) % 4; s_c = nr == 3 ? min(3, s_c * 4 / 10) : (s_r == 3 ? s_c * 10 / 4 + 1 : s_c); s_r = nr; if (s_c > 9) s_c = 9; fbNav(); }
  if (btnRepeat(BTN_UP))    { int nr = (s_r + 3) % 4; s_c = nr == 3 ? min(3, s_c * 4 / 10) : (s_r == 3 ? s_c * 10 / 4 + 1 : s_c); s_r = nr; if (s_c > 9) s_c = 9; fbNav(); }
  if (btnTap(BTN_SELECT)) {
    size_t n = strlen(s_txt);
    if (s_r < 3) {
      if (n + 1 < s_cap) { s_txt[n] = PAGE[s_page][s_r * 10 + s_c]; s_txt[n + 1] = 0; s_typedAt = millis(); fbNav(); }
      else fbEdge();
    } else switch (s_c) {
      case K_PAGE:  s_page = (s_page + 1) % 4; fbNav(); break;
      case K_SPACE: if (n + 1 < s_cap) { s_txt[n] = ' '; s_txt[n + 1] = 0; s_typedAt = millis(); fbNav(); } break;
      case K_DEL:   if (n) { s_txt[n - 1] = 0; fbNav(); } else fbEdge(); break;
      case K_OK:    fbConfirm(); finish(true); return;
    }
  }
  // ---- draw
  u8g2.setFont(F_TINY);
  u8g2.drawStr(0, 5, s_title);
  char cnt[12]; snprintf(cnt, sizeof(cnt), "%u/%u", (unsigned)strlen(s_txt), (unsigned)(s_cap - 1)); uiRight(127, 5, cnt);
  u8g2.drawFrame(0, 7, 128, 11);
  char show[160]; size_t n = strlen(s_txt);
  for (size_t i = 0; i < n; i++) show[i] = (s_mask && !(i == n - 1 && millis() - s_typedAt < 900)) ? '*' : s_txt[i];
  show[n] = 0;
  u8g2.setFont(F_SMALL);
  const char* p = show; while (u8g2.getStrWidth(p) > 118 && *p) p++;     // keep the end visible
  u8g2.drawStr(3, 15, p);
  int cx = 3 + u8g2.getStrWidth(p);
  if ((millis() / 400) & 1) u8g2.drawVLine(cx + 1, 9, 8);                 // caret
  for (int r = 0; r < 3; r++) for (int c = 0; c < 10; c++) {
    int x = c * 12 + 4, y = 20 + r * 10;
    char ch[2] = { PAGE[s_page][r * 10 + c], 0 };
    bool sel = r == s_r && c == s_c;
    if (sel) { u8g2.drawBox(x - 2, y, 11, 10); u8g2.setDrawColor(0); }
    u8g2.drawStr(x + (ch[0] == ' ' ? 0 : 1), y + 8, ch[0] == ' ' ? "_" : ch);
    u8g2.setDrawColor(1);
  }
  static const char* SPEC[4] = { "", "space", "del", "OK" };
  for (int c = 0; c < 4; c++) {
    int x = c * 32, y = 52;
    bool sel = s_r == 3 && s_c == c;
    const char* t = c == 0 ? PAGE_NAME[(s_page + 1) % 4] : SPEC[c];
    if (sel) { u8g2.drawRBox(x + 1, y, 30, 12, 2); u8g2.setDrawColor(0); } else u8g2.drawRFrame(x + 1, y, 30, 12, 2);
    u8g2.drawStr(x + 16 - u8g2.getStrWidth(t) / 2, y + 9, t);
    u8g2.setDrawColor(1);
  }
}

// ---- unlock code
static bool s_codeOn = false;
static char s_code[8]; static uint8_t s_codeN = 0;
static char s_first[8]; static bool s_confirm = false;
static char s_codeTitle[24];
static void (*s_codeDone)(const char*) = nullptr;
void codeOpen(const char* title, void (*done)(const char*)) {
  s_codeOn = true; s_codeN = 0; s_code[0] = 0; s_confirm = false; s_codeDone = done;
  strlcpy(s_codeTitle, title, sizeof(s_codeTitle));
  inputClearFrame();
}
bool codeActive() { return s_codeOn; }
static void codeFinish(const char* c) { s_codeOn = false; inputClearFrame(); auto cb = s_codeDone; s_codeDone = nullptr; if (cb) cb(c); }
void codeUpdate() {
  if (wantBack()) { fbBack(); codeFinish(nullptr); return; }
  static const char L[4] = { 'U', 'D', 'L', 'R' };
  for (uint8_t b = 0; b < 4; b++) if (btnPressed(b) && s_codeN < 6) { s_code[s_codeN++] = L[b]; s_code[s_codeN] = 0; fbNav(); }
  if (btnTap(BTN_SELECT)) {
    if (s_codeN < 4) { fbError(); toast("4 to 6 presses"); }
    else if (!s_confirm) { strlcpy(s_first, s_code, sizeof(s_first)); s_confirm = true; s_codeN = 0; s_code[0] = 0; fbSelect(); }
    else if (strcmp(s_first, s_code) == 0) { fbConfirm(); codeFinish(s_code); return; }
    else { fbError(); toast("Didn't match"); s_confirm = false; s_codeN = 0; s_code[0] = 0; }
  }
  uiHeader(s_codeTitle);
  u8g2.setFont(F_SMALL);
  uiCenter(26, s_confirm ? "Again to confirm" : "Press 4-6 directions");
  for (int i = 0; i < 6; i++) {
    int cx = 24 + i * 16;
    if (i < s_codeN) u8g2.drawDisc(cx, 40, 4); else if (i < 4) u8g2.drawCircle(cx, 40, 4); else u8g2.drawCircle(cx, 40, 2);
  }
  u8g2.setFont(F_TINY);
  uiCenter(56, "SEL done   hold SEL cancel");
}

#ifdef POTAHTO_SIM
// simulator hook: type text into an open keyboard
void kbdTestType(const char* t) { if (s_on) strlcat(s_txt, t, s_cap); }
#endif
