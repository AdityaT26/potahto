// WiFi status, setup mode, QR codes, web PIN
//
// Keeps WiFi up while open (netAcquire), so in "On demand" mode this is the
// screen to open when you want to use the web app over WiFi.
//   LEFT/RIGHT  pages: Status / QR / PIN
//   UP/DOWN     WiFi mode: Off / On demand / Always on
//   SELECT      setup mode on/off (own hotspot + captive portal)
//   hold SELECT back
#include "Multitool.h"

static uint8_t s_page = 0;     // 0 status, 1 QR, 2 PIN
static const char* MODE_NAME[WM_COUNT] = { "Off", "On demand", "Always on" };

void connectEnter() { netAcquire(); s_page = 0; }
void connectExit()  { netRelease(); settingsSaveSoon(); }

static void drawStatus() {
  uiHeader("Connect");
  u8g2.setFont(F_UI);
  if (netSetupAPActive()) {
    uiCenter(25, "Setup mode");
    u8g2.setFont(F_SMALL);
    char l[40];
    snprintf(l, sizeof(l), "Join %s", netApSsid()); uiCenter(36, l);
    snprintf(l, sizeof(l), "Password %s", netApPass()); uiCenter(45, l);
    uiCenter(54, "QR on next page >");
  } else if (netOnline()) {
    char l[40];
    uiTextClipped(2, 24, netSsid(), 124);
    u8g2.setFont(F_SMALL);
    snprintf(l, sizeof(l), "%s.local", netHostname()); u8g2.drawStr(2, 35, l);
    snprintf(l, sizeof(l), "%s   %d dBm", netIp(), netRssi()); u8g2.drawStr(2, 44, l);
    snprintf(l, sizeof(l), "WiFi: %s", MODE_NAME[S.wifiMode % WM_COUNT]); u8g2.drawStr(2, 53, l);
  } else {
    uiCenter(26, netStateName());
    u8g2.setFont(F_SMALL);
    char l[32]; snprintf(l, sizeof(l), "WiFi: %s", MODE_NAME[S.wifiMode % WM_COUNT]); uiCenter(38, l);
    if (S.wifiMode == WM_OFF)            uiCenter(48, "UP/DOWN to turn on");
    else if (netState() == NET_NO_CONFIG) uiCenter(48, "SEL = setup mode");
    else                                 uiCenter(48, "USB config always works");
  }
  u8g2.setFont(F_TINY);
  uiCenter(63, "SEL setup  U/D mode  </> page");
}

static void drawQR() {
  char txt[96];
  const char *l1 = "", *l2 = "", *l3 = "";
  if (netSetupAPActive()) {
    snprintf(txt, sizeof(txt), "WIFI:T:WPA;S:%s;P:%s;;", netApSsid(), netApPass());
    l1 = "Scan to"; l2 = "join setup"; l3 = "WiFi";
  } else if (netOnline()) {
    snprintf(txt, sizeof(txt), "http://%s/", netIp());
    l1 = "Scan to"; l2 = "open the"; l3 = "web app";
  } else {
    uiHeader("Connect");
    u8g2.setFont(F_SMALL);
    uiCenter(32, "Not connected");
    uiCenter(44, "SEL = setup mode");
    return;
  }
  uiQR(0, 0, 2, txt);                              // 29 modules * 2 + quiet zone = 66 max
  u8g2.setFont(F_SMALL);
  u8g2.drawStr(68, 20, l1); u8g2.drawStr(68, 30, l2); u8g2.drawStr(68, 40, l3);
  u8g2.setFont(F_TINY);
  u8g2.drawStr(68, 54, netIp());
}

static void drawPin() {
  uiHeader("Web PIN");
  u8g2.setFont(F_BIG);
  char p[12]; snprintf(p, sizeof(p), "%.3s %.3s", S.webPin, S.webPin + 3);
  uiCenter(45, p);
  u8g2.setFont(F_TINY);
  uiCenter(56, "enter in the web app (WiFi)");
  uiCenter(63, "also the Arduino OTA password");
}

void connectLoop() {
  if (wantBack()) { fbBack(); goBack(); return; }
  if (btnRepeat(BTN_RIGHT)) { s_page = (s_page + 1) % 3; fbNav(); }
  if (btnRepeat(BTN_LEFT))  { s_page = (s_page + 2) % 3; fbNav(); }
  if (btnRepeat(BTN_UP))    { S.wifiMode = (S.wifiMode + 1) % WM_COUNT; settingsSaveSoon(); fbNav(); toast(MODE_NAME[S.wifiMode]); }
  if (btnRepeat(BTN_DOWN))  { S.wifiMode = (S.wifiMode + WM_COUNT - 1) % WM_COUNT; settingsSaveSoon(); fbNav(); toast(MODE_NAME[S.wifiMode]); }
  if (btnTap(BTN_SELECT)) {
    bool on = !netSetupAPActive();
    netSetupAP(on);
    if (on) { fbConfirm(); s_page = 1; } else fbCancel();
  }

  if (netOnline())              npxSetBase(NPX_SOLID, strip.Color(0, 120, 40), 3);
  else if (netSetupAPActive())  npxSetBase(NPX_BREATHE, strip.Color(0, 80, 200), 4);
  else                          npxSetBase(NPX_BREATHE, strip.Color(120, 80, 0), 4);

  if (s_page == 0) drawStatus();
  else if (s_page == 1) drawQR();
  else drawPin();
}
