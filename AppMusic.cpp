// Music: play songs on the buzzer and compose your own
//
// Library   UP/DOWN pick   SELECT play / stop   RIGHT edit (built-ins: edit a copy)
//           "+ New song" composes from scratch   hold SELECT back
// Composer  LEFT/RIGHT move (RIGHT past the end adds a note)
//           UP/DOWN pitch (a semitone)          SELECT note length 1/16..1/1
//           hold SELECT  song menu: play, insert / delete, rest, dot,
//                        octave, tempo, rename, save, exit
// Up to 16 songs x 512 notes. Paste RTTTL ringtones or use the piano-roll
// Composer in the web app; songs can be alarm / timer / message / boot tones.
#include "Multitool.h"

static Song  s_song;                    // the one being edited
static int   s_slot = 0;                // 0 = not saved yet
static bool  s_editing = false, s_dirty = false;
static int   s_cur = 0, s_view = 0;
static int   s_sel = 0, s_top = 0;
static bool  s_playingAll = false;

// ---- library
static int libItems(int* v) {             // 0 = new, 1..16 user, 100+i built-in
  int m = 0; v[m++] = 0;
  for (int i = 1; i <= SONG_SLOTS; i++) if (songName(i)[0]) v[m++] = i;
  for (int i = 0; i < builtinCount(); i++) v[m++] = 100 + i;
  return m;
}
static void newSong() {
  memset(&s_song, 0, sizeof(s_song));
  snprintf(s_song.name, sizeof(s_song.name), "My song %d", songCount() + 1);
  s_song.bpm = 120;
  static const uint8_t M[4] = { 72, 76, 79, 84 };
  for (int i = 0; i < 4; i++) s_song.notes[i] = { M[i], 4, false };
  s_song.n = 4; s_slot = 0; s_cur = 0; s_view = 0; s_editing = true; s_dirty = true;
}
static void openForEdit(int v) {
  if (v >= 100) { rtttlParse(builtinRtttl(v - 100), s_song); s_slot = 0; strlcat(s_song.name, " copy", sizeof(s_song.name)); s_dirty = true; }
  else { if (!songLoad(v, s_song)) { fbError(); return; } s_slot = v; s_dirty = false; }
  s_cur = 0; s_view = 0; s_editing = true;
}
static int s_delSlot = 0;
static void delYes() { songDelete(s_delSlot); toast("Deleted"); }

void musicEnter() { s_editing = false; }
void musicExit()  { songStop(); }

static void libLoop() {
  static int v[1 + SONG_SLOTS + 32];
  int m = libItems(v);
  if (wantBack()) { songStop(); fbBack(); goBack(); return; }
  if (btnRepeat(BTN_DOWN)) { if (s_sel < m - 1) { s_sel++; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { if (s_sel > 0) { s_sel--; fbNav(); } else fbEdge(); }
  int it = v[min(s_sel, m - 1)];
  if (btnTap(BTN_SELECT)) {
    if (it == 0) { newSong(); fbSelect(); return; }
    if (songPlaying()) songStop();
    else if (it >= 100) songPlayRtttl(builtinRtttl(it - 100), false);
    else songPlaySlot(it, false);
  }
  if (btnTap(BTN_RIGHT)) { if (it == 0) newSong(); else openForEdit(it); fbSelect(); return; }
  if (btnLong(BTN_LEFT) && it > 0 && it < 100) { s_delSlot = it; modalConfirm("Delete", songName(it), delYes); return; }
  const int rows = 3;
  if (s_sel < s_top) s_top = s_sel;
  if (s_sel >= s_top + rows) s_top = s_sel - rows + 1;
  uiHeader("Music");
  for (int r = 0; r < rows; r++) {
    int i = s_top + r; if (i >= m) break;
    int x = v[i];
    const char* name = x == 0 ? "+ New song" : x >= 100 ? builtinName(x - 100) : songName(x);
    uiListRow(r, name, x >= 100 ? "built-in" : "", i == s_sel);
  }
  uiScrollbar(m, s_top, rows);
  if (songPlaying()) { u8g2.setDrawColor(0); u8g2.drawBox(0, 0, 40, 11); u8g2.setDrawColor(1); u8g2.setFont(F_UI); u8g2.drawStr(2, 9, "Playing"); uiGlyph(GLYPH_MUSIC, 44, 0, 10); }
}

// ---- composer
static const char* NOTE_NAME[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static void preview(const SongNote& n) { if (n.midi) buzzTone(noteFreq(n.midi), 90); }
static void tempoDone() { s_dirty = true; }
static int  s_bpm;
static void tempoChange(int v) { s_song.bpm = (uint16_t)v; }
static void renameDone(bool ok) { if (ok) s_dirty = true; }
static void saveNow(bool exitAfter) {
  if (songSave(s_slot, s_song)) { s_slot = songLastSaved(); s_dirty = false; toast("Saved"); fbConfirm(); if (exitAfter) { s_editing = false; songStop(); } }
  else { toast(s_slot ? "Save failed" : "16 songs max"); fbError(); }
}
static void discardYes() { s_editing = false; songStop(); }
static void menuPick(int i) {
  SongNote& n = s_song.notes[s_cur];
  switch (i) {
    case 0: s_playingAll = false; songPlayStruct(s_song, false, s_cur); break;              // play from here
    case 1: s_playingAll = true; songPlayStruct(s_song, false, 0); break;                   // play all
    case 2: if (s_song.n < SONG_MAX_NOTES) { memmove(&s_song.notes[s_cur + 1], &s_song.notes[s_cur], (s_song.n - s_cur) * sizeof(SongNote)); s_song.n++; s_cur++; s_dirty = true; } break;
    case 3: if (s_song.n > 1) { memmove(&s_song.notes[s_cur], &s_song.notes[s_cur + 1], (s_song.n - s_cur - 1) * sizeof(SongNote)); s_song.n--; if (s_cur >= s_song.n) s_cur = s_song.n - 1; s_dirty = true; } break;
    case 4: n.midi = n.midi ? 0 : 72; s_dirty = true; break;                              // rest <-> note
    case 5: n.dot = !n.dot; s_dirty = true; break;
    case 6: if (n.midi && n.midi + 12 <= 108) { n.midi += 12; preview(n); s_dirty = true; } break;
    case 7: if (n.midi && n.midi - 12 >= 48) { n.midi -= 12; preview(n); s_dirty = true; } break;
    case 8: s_bpm = s_song.bpm; modalSlider("Tempo (BPM)", &s_bpm, 40, 300, 5, "", tempoChange, tempoDone); break;
    case 9: kbdOpen("Song name", s_song.name, sizeof(s_song.name), false, renameDone); break;
    case 10: saveNow(false); break;
    case 11: saveNow(true); break;
    case 12: if (s_dirty) modalConfirm("Exit", "Drop the changes?", discardYes); else discardYes(); break;
  }
}

static void editLoop() {
  if (wantBack()) {
    static const char* const M[] = { "Play from here", "Play all", "Insert note", "Delete note", "Rest <-> note",
                                     "Dotted on/off", "Octave up", "Octave down", "Tempo", "Rename", "Save", "Save & exit", "Exit" };
    songStop(); modalChoice("Song", M, 13, 0, menuPick); return;
  }
  SongNote& n = s_song.notes[s_cur];
  if (btnRepeat(BTN_RIGHT)) {
    songStop();
    if (s_cur < s_song.n - 1) s_cur++;
    else if (s_song.n < SONG_MAX_NOTES) { s_song.notes[s_song.n] = s_song.notes[s_song.n - 1]; s_song.n++; s_cur++; s_dirty = true; }
    else fbEdge();
    fbNav();
  }
  if (btnRepeat(BTN_LEFT)) { songStop(); if (s_cur > 0) { s_cur--; fbNav(); } else fbEdge(); }
  if (btnRepeat(BTN_UP))   { songStop(); if (!n.midi) n.midi = s_cur ? max<uint8_t>(48, s_song.notes[s_cur - 1].midi) : 72; else if (n.midi < 108) n.midi++; preview(n); s_dirty = true; }
  if (btnRepeat(BTN_DOWN)) { songStop(); if (!n.midi) n.midi = 72; else if (n.midi > 48) n.midi--; preview(n); s_dirty = true; }
  if (btnTap(BTN_SELECT))  {
    static const uint8_t L[6] = { 16, 8, 4, 2, 1, 32 };
    int k = 0; for (int i = 0; i < 6; i++) if (L[i] == n.len) k = i;
    n.len = L[(k + 1) % 6]; preview(n); s_dirty = true;
  }
  int playIdx = songPlaying() ? songPosition() : -1;
  int focus = playIdx >= 0 ? playIdx : s_cur;

  // ---- draw: piano roll
  char h[32]; snprintf(h, sizeof(h), "%s%s", s_dirty ? "*" : "", s_song.name);
  uiHeader(h);
  const int y0 = 13, hgt = 36, cw = 8;            // 15 visible cells
  if (focus < s_view + 2) s_view = max(0, focus - 2);
  if (focus > s_view + 12) s_view = focus - 12;
  int center = s_song.notes[focus].midi ? s_song.notes[focus].midi : 72;
  for (int c = 0; c < 15; c++) {
    int i = s_view + c; if (i >= s_song.n) break;
    const SongNote& sn = s_song.notes[i];
    int x = c * cw + 4;
    int w = sn.len >= 16 ? 3 : sn.len == 8 ? 4 : sn.len == 4 ? 5 : sn.len == 2 ? 6 : 7;
    if (sn.len == 32) w = 2;
    if (sn.midi) {
      int y = y0 + hgt / 2 - (sn.midi - center) * 2;
      y = clampv(y, y0, y0 + hgt - 3);
      u8g2.drawBox(x, y, w, 3);
      if (sn.dot) u8g2.drawPixel(x + w + 1, y + 1);
    } else { u8g2.drawHLine(x, y0 + hgt - 2, w); }
    if (i == s_cur) u8g2.drawFrame(x - 2, y0, w + 4, hgt);
    if (i == playIdx) u8g2.drawVLine(x + w / 2, y0, 2);
  }
  // info line
  u8g2.drawHLine(0, y0 + hgt + 1, 128);
  u8g2.setFont(F_SMALL);
  char info[40];
  if (n.midi) snprintf(info, sizeof(info), "%s%d 1/%d%s", NOTE_NAME[n.midi % 12], n.midi / 12 - 1, n.len, n.dot ? "." : "");
  else snprintf(info, sizeof(info), "rest 1/%d%s", n.len, n.dot ? "." : "");
  u8g2.drawStr(0, 58, info);
  char pos[24]; snprintf(pos, sizeof(pos), "#%d of %d  %u bpm", s_cur + 1, s_song.n, s_song.bpm);
  uiRight(127, 58, pos);
  u8g2.setFont(F_TINY);
  uiCenter(63, "U/D pitch  SEL len  hold SEL menu");
}

void musicLoop() { if (s_editing) editLoop(); else libLoop(); }
