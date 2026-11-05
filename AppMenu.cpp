// Two-level main menu: folder carousel -> app list
//
// Carousel : LEFT/RIGHT (or UP/DOWN) pick a folder, SELECT opens it.
//            A folder holding a single app (Games) opens that app directly.
// List     : UP/DOWN pick, SELECT/RIGHT open, LEFT or hold SELECT = back to
//            the carousel. Returning from an app lands back in its list.
#include "Multitool.h"

struct Folder { uint8_t cat; const char* name; uint8_t glyph; };
static const Folder FOLDERS[] = {
  { CAT_TOOLS,    "Tools",    GLYPH_TOOL     },
  { CAT_CONTROL,  "Control",  GLYPH_KEYS     },
  { CAT_WIRELESS, "Wireless", GLYPH_WIFI     },
  { CAT_SMART,    "Smart",    GLYPH_HOME     },
  { CAT_DESK,     "Desk",     GLYPH_CLOCK    },
  { CAT_GAMES,    "Games",    GLYPH_GAMES    },
  { CAT_SYSTEM,   "System",   GLYPH_SETTINGS },
};
static const int NFOLD = (int)ARRAY_LEN(FOLDERS);

static int  s_fold = 0;          // index into FOLDERS
static bool s_inList = false;
static int  s_sel = 0, s_top = 0;

static int appsIn(uint8_t cat, uint8_t* out) {
  int n = 0;
  for (int i = 1; i < APP_COUNT; i++) if (APPS[i].cat == cat) { if (out) out[n] = (uint8_t)i; n++; }
  return n;
}
static void stepFolder(int dir) {
  for (int k = 0; k < NFOLD; k++) {               // skip empty folders
    s_fold = (s_fold + dir + NFOLD) % NFOLD;
    if (appsIn(FOLDERS[s_fold].cat, nullptr) > 0) return;
  }
}

void menuEnter() {
  npxSetBase(NPX_BREATHE, npxHSV(28, 255, 255), 3);   // warm ambient idle
  if (appsIn(FOLDERS[s_fold].cat, nullptr) == 0) stepFolder(+1);
}

static void open(uint8_t id) { fbSelect(); btnConsume(BTN_SELECT); openApp(id); }

static void carouselLoop() {
  if (btnRepeat(BTN_RIGHT) || btnRepeat(BTN_DOWN)) { stepFolder(+1); fbNav(); }
  if (btnRepeat(BTN_LEFT)  || btnRepeat(BTN_UP))   { stepFolder(-1); fbNav(); }
  if (btnTap(BTN_SELECT)) {                       // tap (not press): holding SELECT here = screen off
    uint8_t ids[APP_COUNT];
    int n = appsIn(FOLDERS[s_fold].cat, ids);
    if (n == 1) { open(ids[0]); return; }
    if (n > 1) { s_inList = true; s_sel = 0; s_top = 0; fbSelect(); btnConsume(BTN_SELECT); }
    return;
  }

  struct tm lt; char title[24];
  if (timeNow(lt)) { char hm[8]; timeFmtHM(hm, sizeof(hm), lt); snprintf(title, sizeof(title), "%s%s", hm, timeAmPm(lt)); }
  else strlcpy(title, S.devName, sizeof(title));
  uiHeader(title);
  u8g2.drawTriangle(9, 23, 9, 35, 3, 29);          // chevrons
  u8g2.drawTriangle(118, 23, 118, 35, 124, 29);
  uiGlyph(FOLDERS[s_fold].glyph, 49, 14, 30);
  u8g2.setFont(F_MEDB);
  uiCenter(57, FOLDERS[s_fold].name);

  // page dots for non-empty folders
  int nonEmpty = 0, mine = 0;
  for (int i = 0; i < NFOLD; i++) if (appsIn(FOLDERS[i].cat, nullptr)) { if (i == s_fold) mine = nonEmpty; nonEmpty++; }
  int cell = 6, sx = (SCR_W - nonEmpty * cell) / 2;
  for (int i = 0; i < nonEmpty; i++) {
    int cxd = sx + i * cell + 3;
    if (i == mine) u8g2.drawDisc(cxd, 62, 1); else u8g2.drawPixel(cxd, 62);
  }
}

static void listLoop() {
  uint8_t ids[APP_COUNT];
  int n = appsIn(FOLDERS[s_fold].cat, ids);
  if (btnLong(BTN_SELECT) || btnTap(BTN_LEFT)) { s_inList = false; fbBack(); return; }
  if (btnRepeat(BTN_DOWN)) { if (s_sel < n - 1) { s_sel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_sel > 0) { s_sel--; fbNav(); } else fbEdge(); }
  if (btnTap(BTN_SELECT) || btnTap(BTN_RIGHT)) { if (n) open(ids[s_sel]); return; }

  const int rows = 3;
  if (s_sel < s_top) s_top = s_sel;
  if (s_sel >= s_top + rows) s_top = s_sel - rows + 1;
  uiHeader(FOLDERS[s_fold].name);
  for (int r = 0; r < rows; r++) {
    int i = s_top + r; if (i >= n) break;
    int top = BODY_Y + r * 17;
    bool sel = (i == s_sel);
    if (sel) { u8g2.drawRBox(0, top, 125, 16, 2); u8g2.setDrawColor(0); }
    uiGlyph(APPS[ids[i]].glyph, 3, top + 1, 14);
    u8g2.setFont(F_UI);
    uiTextClipped(22, top + 12, APPS[ids[i]].name, 100);
    if (sel) u8g2.setDrawColor(1);
  }
  uiScrollbar(n, s_top, rows);
}

void menuLoop() {
  if (s_inList) listLoop(); else carouselLoop();
}
