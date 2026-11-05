// WiFi Scan (channels, hunter), BLE Scan (finder), LAN Scan, Net Doctor, Wake PC
#include "Multitool.h"

static void bars(int x, int yb, int rssi) {             // 4 bars, 9x7
  int n = rssi > -55 ? 4 : rssi > -67 ? 3 : rssi > -78 ? 2 : rssi > -90 ? 1 : 0;
  for (int i = 0; i < 4; i++) { int h = 2 + i * 2; if (i < n) u8g2.drawBox(x + i * 2 + i, yb - h, 2, h); else u8g2.drawPixel(x + i * 3, yb - 1); }
}
static const char* encName(uint8_t e) {
  static const char* N[] = { "open", "WEP", "WPA", "WPA2", "WPA/WPA2", "WPA2-Ent", "WPA3", "WPA2/WPA3", "WAPI", "OWE" };
  return e < sizeof(N) / sizeof(N[0]) ? N[e] : "?";
}
static bool wifiOffGate(const char* title) {          // true = handled (WiFi is switched off)
  if (S.wifiMode != WM_OFF) return false;
  if (btnTap(BTN_SELECT)) { S.wifiMode = WM_ON_DEMAND; settingsSaveSoon(); netAcquire(); fbConfirm(); return true; }
  uiHeader(title);
  u8g2.setFont(F_SMALL); uiCenter(32, "WiFi is off in Settings");
  u8g2.setFont(F_TINY);  uiCenter(46, "SEL: switch to On demand");
  return true;
}

//  WiFi Scan
static uint8_t  s_wPage = 0;         // 0 list, 1 channels, 2 details, 3 hunter
static int      s_wSel = 0, s_wTop = 0;
static uint32_t s_wLastScan = 0, s_hBeep = 0;
static NetAP    s_wAp;
static int      s_hPeak = -127;
static int8_t   s_hHist[64]; static int s_hHistN = 0;
static bool     s_hSound = true;

void wifiScanEnter() { s_wPage = 0; s_wSel = 0; s_wTop = 0; netAcquire(); s_wLastScan = 0; }
void wifiScanExit()  { hunterStop(); netRelease(); }

static void drawChannels(const NetAP* a, int n) {
  float score[14] = { 0 };
  for (int i = 0; i < n; i++) {
    if (a[i].channel < 1 || a[i].channel > 13) continue;
    float w = max(0, a[i].rssi + 100);
    for (int c = 1; c <= 13; c++) if (abs(c - a[i].channel) <= 2) score[c] += w * (abs(c - a[i].channel) == 2 ? 0.3f : abs(c - a[i].channel) == 1 ? 0.7f : 1.0f);
  }
  float mx = 1; for (int c = 1; c <= 13; c++) mx = max(mx, score[c]);
  int best = 1; for (int c : { 6, 11 }) if (score[c] < score[best]) best = c;
  uiHeader("Channels 2.4G");
  for (int c = 1; c <= 13; c++) {
    int x = 4 + (c - 1) * 9, h = (int)(34 * score[c] / mx);
    if (c == best) u8g2.drawFrame(x, 52 - h, 7, max(1, h)); else u8g2.drawBox(x, 52 - h, 7, max(1, h));
    char t[3]; snprintf(t, sizeof(t), "%d", c); u8g2.setFont(F_TINY); u8g2.drawStr(x + (c < 10 ? 2 : 0), 59, t);
  }
  const uint8_t* b = netBssid();
  char l[28]; snprintf(l, sizeof(l), "best: %d", best); u8g2.setFont(F_TINY); u8g2.drawStr(2, 19, l);
  if (b) { snprintf(l, sizeof(l), "yours: %d", netChannel()); uiRight(126, 19, l); }
}

void wifiScanLoop() {
  if (wifiOffGate("WiFi Scan")) return;
  uint32_t now = millis();
  int n; const NetAP* a = netScanResults(&n);
  if (s_wPage <= 1 && n != -1 && (now - s_wLastScan > 8000 || !s_wLastScan)) { netScanStart(); s_wLastScan = now; }

  if (s_wPage == 3) {                                    // ---- hunter ----
    if (wantBack() || btnTap(BTN_LEFT)) { hunterStop(); s_wPage = 2; fbBack(); return; }
    if (btnTap(BTN_UP)) { s_hSound = !s_hSound; toast(s_hSound ? "Sound on" : "Sound off"); }
    if (btnTap(BTN_SELECT)) { s_hPeak = -127; fbNav(); }
    int r = hunterRssi();
    static uint32_t lastHist = 0;
    if (now - lastHist > 250) { lastHist = now; if (s_hHistN < 64) s_hHist[s_hHistN++] = r; else { memmove(s_hHist, s_hHist + 1, 63); s_hHist[63] = r; } }
    if (r > s_hPeak) s_hPeak = r;
    if (r > -127 && s_hSound) {
      uint32_t gap = (uint32_t)map(clampv(r, -95, -35), -95, -35, 1200, 70);   // Geiger: closer = faster
      if (now - s_hBeep > gap) { s_hBeep = now; buzzTone(2600, 12); }
    }
    uiHeader(hunterSsid()[0] ? hunterSsid() : "Hunter");
    char v[8]; if (r > -127) snprintf(v, sizeof(v), "%d", r); else strcpy(v, "--");
    uiBigValue(34, 42, v, "dBm");
    char pk[16]; snprintf(pk, sizeof(pk), "peak %d", s_hPeak > -127 ? s_hPeak : 0); u8g2.setFont(F_TINY); u8g2.drawStr(80, 20, pk);
    for (int i = 1; i < s_hHistN; i++) {                // sparkline, right half
      int x0 = 70 + (i - 1) * 56 / 63, x1 = 70 + i * 56 / 63;
      int y0 = 46 - (clampv<int>(s_hHist[i - 1], -100, -30) + 100) * 20 / 70, y1 = 46 - (clampv<int>(s_hHist[i], -100, -30) + 100) * 20 / 70;
      u8g2.drawLine(x0, y0, x1, y1);
    }
    uiProgressBar(2, 49, 124, 7, r > -127 ? (r + 100) / 70.0f : 0);
    u8g2.setFont(F_TINY); uiCenter(63, "SEL reset peak   UP sound");
    npxSetBase(NPX_SOLID, npxHSV((uint8_t)map(clampv(r, -95, -35), -95, -35, 0, 85), 255, 160), 3);
    return;
  }
  if (s_wPage == 2) {                                    // ---- details ----
    if (wantBack() || btnTap(BTN_LEFT)) { s_wPage = 0; fbBack(); return; }
    if (btnTap(BTN_SELECT)) { hunterStart(s_wAp.bssid, s_wAp.channel, s_wAp.ssid); s_hPeak = -127; s_hHistN = 0; s_wPage = 3; fbSelect(); return; }
    uiHeader(s_wAp.ssid[0] ? s_wAp.ssid : "(hidden)");
    u8g2.setFont(F_SMALL);
    char l[32];
    snprintf(l, sizeof(l), "%02X:%02X:%02X:%02X:%02X:%02X", s_wAp.bssid[0], s_wAp.bssid[1], s_wAp.bssid[2], s_wAp.bssid[3], s_wAp.bssid[4], s_wAp.bssid[5]); u8g2.drawStr(2, 22, l);
    snprintf(l, sizeof(l), "Channel %d", s_wAp.channel); u8g2.drawStr(2, 31, l);
    snprintf(l, sizeof(l), "Signal %d dBm", s_wAp.rssi); u8g2.drawStr(2, 40, l);
    snprintf(l, sizeof(l), "Security %s", encName(s_wAp.enc)); u8g2.drawStr(2, 49, l);
    u8g2.setFont(F_TINY); uiCenter(63, "SEL hunt signal   < back");
    return;
  }
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnTap(BTN_RIGHT)) { s_wPage ^= 1; fbNav(); }
  if (btnTap(BTN_LEFT) && s_wPage == 1) { s_wPage = 0; fbNav(); }
  if (s_wPage == 1) { drawChannels(a, max(n, 0)); return; }

  int cnt = max(n, 0);
  if (btnRepeat(BTN_DOWN)) { if (s_wSel < cnt - 1) { s_wSel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_wSel > 0) { s_wSel--; fbNav(); } else fbEdge(); }
  if (btnTap(BTN_SELECT) && cnt) { s_wAp = a[min(s_wSel, cnt - 1)]; s_wPage = 2; fbSelect(); return; }
  s_wSel = min(s_wSel, max(cnt - 1, 0));
  const int rows = 5;
  if (s_wSel < s_wTop) s_wTop = s_wSel;
  if (s_wSel >= s_wTop + rows) s_wTop = s_wSel - rows + 1;
  char title[24]; snprintf(title, sizeof(title), n == -1 ? "WiFi: scanning" : "WiFi: %d nets", cnt);
  uiHeader(title);
  if (!cnt) { u8g2.setFont(F_SMALL); uiCenter(36, n == -1 || n == -2 ? "Scanning..." : "Nothing found"); return; }
  u8g2.setFont(F_SMALL);
  for (int r = 0; r < rows; r++) {
    int i = s_wTop + r; if (i >= cnt) break;
    int y = 21 + r * 10;
    bool sel = i == s_wSel;
    if (sel) { u8g2.drawBox(0, y - 8, 125, 10); u8g2.setDrawColor(0); }
    bars(2, y, a[i].rssi);
    uiTextClipped(16, y, a[i].ssid[0] ? a[i].ssid : "(hidden)", 84);
    char c[6]; snprintf(c, sizeof(c), "%d", a[i].channel); uiRight(123, y, c);
    if (a[i].enc == 0) u8g2.drawStr(102, y, "o");
    if (sel) u8g2.setDrawColor(1);
  }
  uiScrollbar(cnt, s_wTop, rows);
}

//  BLE Scan + Finder
static BleDev   s_bl[BLE_MAX];
static int      s_blN = 0, s_bSel = 0, s_bTop = 0;
static uint32_t s_bSnap = 0, s_fBeep = 0;
static bool     s_finder = false;
static BleDev   s_target;

void bleScanEnter() { bleScanStart(); s_finder = false; s_bSel = 0; s_bTop = 0; }
void bleScanExit()  { bleScanStop(); }

static void bleLabel(const BleDev& d, char* o, size_t n) {
  if (d.name[0]) { strlcpy(o, d.name, n); return; }
  const char* k = bleKindName(d.kind); const char* c = bleCompanyName(d.company);
  if (k[0]) snprintf(o, n, "%s%s%s", c ? c : "", c ? " " : "", k);
  else if (c) snprintf(o, n, "%s device", c);
  else snprintf(o, n, "%02X:%02X:%02X:%02X", d.addr[5], d.addr[4], d.addr[3], d.addr[2]);
}

void bleScanLoop() {
  uint32_t now = millis();
  if (now - s_bSnap > 500) { s_bSnap = now; s_blN = bleSnapshot(s_bl, BLE_MAX); }
  if (s_finder) {
    if (wantBack() || btnTap(BTN_LEFT)) { s_finder = false; fbBack(); return; }
    BleDev d; bool have = bleLookup(s_target.addr, d) && now - d.seen < 6000;
    int r = have ? (int)roundf(d.rssiAvg) : -127;
    if (have) {
      uint32_t gap = (uint32_t)map(clampv(r, -100, -40), -100, -40, 1400, 80);
      if (now - s_fBeep > gap) { s_fBeep = now; buzzTone(2200, 12); motorPulseStr(8, 3); }
    }
    char lab[24]; bleLabel(s_target, lab, sizeof(lab));
    uiHeader(lab);
    char v[8]; if (have) snprintf(v, sizeof(v), "%d", r); else strcpy(v, "--");
    uiBigValue(48, 42, v, "dBm");
    const char* hint = !have ? "lost it - keep moving" : r > -50 ? "very close!" : r > -65 ? "close" : r > -80 ? "nearby" : "far";
    u8g2.setFont(F_SMALL); uiCenter(53, hint);
    uiProgressBar(2, 56, 124, 7, have ? (r + 100) / 60.0f : 0);
    npxSetBase(NPX_SOLID, npxHSV((uint8_t)map(clampv(r, -100, -40), -100, -40, 0, 85), 255, 160), 3);
    return;
  }
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnRepeat(BTN_DOWN)) { if (s_bSel < s_blN - 1) { s_bSel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_bSel > 0) { s_bSel--; fbNav(); } else fbEdge(); }
  if (btnTap(BTN_SELECT) && s_blN) { s_target = s_bl[min(s_bSel, s_blN - 1)]; s_finder = true; fbSelect(); return; }
  s_bSel = min(s_bSel, max(s_blN - 1, 0));
  const int rows = 5;
  if (s_bSel < s_bTop) s_bTop = s_bSel;
  if (s_bSel >= s_bTop + rows) s_bTop = s_bSel - rows + 1;
  char t[24]; snprintf(t, sizeof(t), "BLE: %d nearby", s_blN);
  uiHeader(t);
  if (!s_blN) { u8g2.setFont(F_SMALL); uiCenter(36, bleScanning() ? "Listening..." : "Bluetooth off"); return; }
  u8g2.setFont(F_SMALL);
  for (int r = 0; r < rows; r++) {
    int i = s_bTop + r; if (i >= s_blN) break;
    int y = 21 + r * 10;
    bool sel = i == s_bSel;
    if (sel) { u8g2.drawBox(0, y - 8, 125, 10); u8g2.setDrawColor(0); }
    char lab[24]; bleLabel(s_bl[i], lab, sizeof(lab));
    uiTextClipped(2, y, lab, 98);
    char v[6]; snprintf(v, sizeof(v), "%d", (int)roundf(s_bl[i].rssiAvg)); uiRight(123, y, v);
    if (sel) u8g2.setDrawColor(1);
  }
  uiScrollbar(s_blN, s_bTop, rows);
}

//  LAN Scan
static LanHost s_lh[LAN_MAX];
static int     s_lhN = 0, s_lSel = 0, s_lTop = 0;
static bool    s_lanStarted = false;
void lanScanEnter() { netAcquire(); s_lanStarted = false; s_lSel = 0; s_lTop = 0; }
void lanScanExit()  { netRelease(); }
void lanScanLoop() {
  if (wifiOffGate("LAN Scan")) return;
  if (wantBack()) { fbBack(); goBack(); return; }
  if (!netOnline()) {
    uiHeader("LAN Scan"); u8g2.setFont(F_SMALL);
    uiCenter(32, netStateName()); u8g2.setFont(F_TINY); uiCenter(46, "waiting for WiFi...");
    return;
  }
  if (!s_lanStarted || btnTap(BTN_RIGHT)) { lanScanStart(); s_lanStarted = true; }
  s_lhN = lanSnapshot(s_lh, LAN_MAX);
  if (btnRepeat(BTN_DOWN)) { if (s_lSel < s_lhN - 1) { s_lSel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_lSel > 0) { s_lSel--; fbNav(); } else fbEdge(); }
  if (btnTap(BTN_SELECT) && s_lhN) {
    const LanHost& h = s_lh[min(s_lSel, s_lhN - 1)];
    char m[44]; snprintf(m, sizeof(m), "%s\n%u.%u.%u.%u :%u", lanServiceName(h.svc), (unsigned)(h.ip & 255), (unsigned)((h.ip >> 8) & 255),
                         (unsigned)((h.ip >> 16) & 255), (unsigned)(h.ip >> 24), h.port);
    modalInfo(h.host, m);
  }
  const int rows = 5;
  if (s_lSel < s_lTop) s_lTop = s_lSel;
  if (s_lSel >= s_lTop + rows) s_lTop = s_lSel - rows + 1;
  char t[24];
  if (lanScanning()) snprintf(t, sizeof(t), "LAN: %d%%", lanProgress()); else snprintf(t, sizeof(t), "LAN: %d found", s_lhN);
  uiHeader(t);
  if (lanScanning()) u8g2.drawHLine(0, 12, 128 * lanProgress() / 100);
  if (!s_lhN) { u8g2.setFont(F_SMALL); uiCenter(36, lanScanning() ? "Asking the network..." : "Nothing announced"); u8g2.setFont(F_TINY); uiCenter(62, "> rescan"); return; }
  u8g2.setFont(F_SMALL);
  for (int r = 0; r < rows; r++) {
    int i = s_lTop + r; if (i >= s_lhN) break;
    int y = 21 + r * 10;
    bool sel = i == s_lSel;
    if (sel) { u8g2.drawBox(0, y - 8, 125, 10); u8g2.setDrawColor(0); }
    uiTextClipped(2, y, s_lh[i].host, 72);
    u8g2.setFont(F_TINY); uiRight(123, y, lanServiceName(s_lh[i].svc)); u8g2.setFont(F_SMALL);
    if (sel) u8g2.setDrawColor(1);
  }
  uiScrollbar(s_lhN, s_lTop, rows);
}

//  Net Doctor
static bool s_docStarted = false;
void doctorEnter() { netAcquire(); s_docStarted = false; }
void doctorExit()  { netRelease(); }
void doctorLoop() {
  if (wifiOffGate("Net Doctor")) return;
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnTap(BTN_SELECT) && !doctorRunning()) { doctorRun(); s_docStarted = true; fbSelect(); }
  if (!s_docStarted && netOnline()) { doctorRun(); s_docStarted = true; }
  uiHeader(doctorRunning() ? "Net Doctor..." : "Net Doctor");
  if (!s_docStarted) { u8g2.setFont(F_SMALL); uiCenter(34, netStateName()); u8g2.setFont(F_TINY); uiCenter(46, "waiting for WiFi..."); return; }
  const DocResult* r = doctorResults();
  for (int i = 0; i < DS_COUNT; i++) {
    int y = 19 + i * 8;
    u8g2.setFont(F_SMALL);
    int cx = 4, cy = y - 3;
    switch (r[i].state) {
      case 1: u8g2.drawLine(cx - 2, cy, cx, cy + 2); u8g2.drawLine(cx, cy + 2, cx + 3, cy - 2); break;      // tick
      case 2: u8g2.drawStr(cx - 1, y, "!"); break;
      case 3: u8g2.drawLine(cx - 2, cy - 2, cx + 2, cy + 2); u8g2.drawLine(cx - 2, cy + 2, cx + 2, cy - 2); break;
      default: if ((millis() / 300) % 2) u8g2.drawPixel(cx, cy); break;
    }
    u8g2.drawStr(11, y, doctorStepName(i));
    u8g2.setFont(F_TINY);
    uiRight(127, y, r[i].note);
  }
  u8g2.setFont(F_TINY); if (!doctorRunning()) uiCenter(63, "SEL run again");
}

//  Wake PC
static int s_wolSel = 0;
void wakeEnter() { netAcquire(); s_wolSel = 0; }
void wakeExit()  { netRelease(); }
void wakeLoop() {
  if (wifiOffGate("Wake PC")) return;
  if (wantBack()) { fbBack(); goBack(); return; }
  int n = wolCount();
  if (btnRepeat(BTN_DOWN)) { if (s_wolSel < n - 1) { s_wolSel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_wolSel > 0) { s_wolSel--; fbNav(); } else fbEdge(); }
  if (btnTap(BTN_SELECT) && n) {
    if (!netOnline()) { toast("WiFi not connected"); fbError(); }
    else if (wolSend(s_wolSel)) { toast("Magic packet sent"); fbConfirm(); }
    else { toast("Send failed"); fbError(); }
  }
  uiHeader("Wake PC");
  if (!n) { u8g2.setFont(F_SMALL); uiCenter(32, "No PCs saved"); u8g2.setFont(F_TINY); uiCenter(44, "add them: web app > Toolkit"); return; }
  int rows = uiListRows(), top = max(0, s_wolSel - rows + 1);
  for (int r = 0; r < rows; r++) { int i = top + r; if (i >= n) break; uiListRow(r, wolName(i), i == s_wolSel ? "wake" : "", i == s_wolSel); }
  if (!netOnline()) { u8g2.setFont(F_TINY); uiCenter(63, netStateName()); }
}
