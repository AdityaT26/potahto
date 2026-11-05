// RTTTL parser/writer, song library (/songs.json), playback and the built-in
// tunes (every game has its own theme).
#include "Multitool.h"
#include "Proto.h"

#define SONG_FILE "/songs.json"
#define RTTTL_MAX 2400

// ---- built-in tunes (all original)
struct Builtin { const char* name; const char* rtttl; };
static const Builtin BUILTIN[] = {
  { "Spud Knight",  "Spud Knight:d=8,o=5,b=140:a4,a4,c,e,4a,g,f,4e,d,c,4d,e,c,4a4,a4,c,e,a,b,c6,4b,a,g#,4a,p,f,f,a,c6,4d6,c6,a#,4a,g,f,4g,a,g,4e,c,d,e,f,g#,b,4a,4p" },
  { "Tater Run",    "Tater Run:d=8,o=5,b=180:c6,p,g,c6,e6,p,d6,c6,a,p,f,a,c6,p,a,c6,g,p,e,g,c6,p,b,c6,d6,p,g,d6,4e6,p,c6,e6,d6,c6,a,c6,f,a,g,e,c,e,g,c6,4c6,4p" },
  { "Spud Defense", "Spud Defense:d=8,o=5,b=120:d,p,d,f,4a,g,f,4e,c,e,4g,f,e,4d,p,d,f,a,d6,c6,a,4b,g,a,b,c6,4a,e,f,g,e,4d,4p" },
  { "Snake",        "Snake:d=8,o=5,b=160:g,a#,c6,c#6,d6,p,a#,g,f,g,p,a#,4d6,c6,a#,g,a#,c6,p,d6,f6,d6,c6,a#,4g,p,f,g,a#,g,f,d,f,4g,4p" },
  { "Pong",         "Pong:d=16,o=6,b=200:c,p,c,p,8g5,8p,c,p,e,p,8g,8p,f,p,e,p,8d,8p,c,p,d,p,8e,8p,g5,p,c,p,8c,8p" },
  { "Blocks",       "Blocks:d=8,o=5,b=150:a,c6,e6,a6,g6,e6,4f6,d6,b,g,b,d6,4e6,c6,a,f,a,c6,f6,e6,d6,c6,b,c6,d6,4e6,4p,a6,g6,f6,e6,d6,f6,4e6,c6,b,a,b,c6,a,4b,4a,4p" },
  { "Flappy",       "Flappy:d=8,o=5,b=130:f,a,c6,f6,4e6,c6,a,4g,a#,4a,f,a,c6,a,4g,e,g,4f,p,c6,d6,c6,a#,a,g,a,a#,4c6,a,f,g,e,4f,4p" },
  { "Dungeon",      "Dungeon:d=8,o=4,b=100:c,d#,g,c5,4b,g,f#,4g,p,g#,g,f,d#,4d,b3,d,4c,p,c,d#,g,c5,d#5,d5,c5,b,4c5,g#,f,4g,d,b3,2c" },
  { "Tilt Maze",    "Tilt Maze:d=4,o=5,b=140:c,e,g,a,g,e,f,a,c6,2b,g,e,g,c6,e6,d6,b,g,2c6,p,a,c6,a,g,e,c,d,f,a,2g,p" },
  { "Tilt Race",    "Tilt Race:d=16,o=5,b=190:a,a,a6,a,a,g6,a,a,f6,a,a,e6,a,a,c6,8d6,g,g,g6,g,g,f6,g,g,e6,g,g,d6,g,g,b,8c6,f,f,f6,f,f,e6,f,f,d6,f,f,c6,8e6,e6,e6,d6,c6,b,a,g#,4a" },
  { "Blaster",      "Blaster:d=16,o=5,b=170:c,e,g,c6,e6,g6,e6,c6,a#4,d,f,a#,d6,f6,d6,a#,g#4,c,d#,g#,c6,d#6,c6,g#,g4,b4,d,g,b,d6,8g6,8p,c,e,g,c6,e6,g6,c7,g6,8e6,8c6,4p" },
  { "Sonar",        "Sonar:d=4,o=5,b=80:8e6,8p,2p,8b5,8p,8e5,8g5,2a5,8e6,8p,2p,8d6,8c6,8b5,8a5,2b5,8e6,8p,8g6,8e6,2d6,8c6,8b5,2a5,2p" },
  { "Rise and Shine","Rise and Shine:d=8,o=6,b=125:c,e,g,c7,4p,c,e,g,c7,4p,a,g,e,c,d,e,4c,p,g5,c,e,g,4e,d,c,d,4e,2c" },
  { "Time's Up",    "Time's Up:d=8,o=6,b=200:c,e,c,e,c,e,4g,p,c,e,c,e,c,e,4g,4p" },
  { "Ding Dong",    "Ding Dong:d=4,o=6,b=180:e,c,d,g5,p,g5,d,e,c" },
  { "Fanfare",      "Fanfare:d=16,o=5,b=140:c,c,c,8c,8p,g4,8p,a4,8p,c,p,4g4,p,c,8e,8g,4c6" },
  { "Night Light",  "Night Light:d=4,o=5,b=80:c,e,g,2e,d,f,a,2f,e,g,c6,b,a,g,2f,e,d,2c" },
};
static const int NBUILTIN = (int)(sizeof(BUILTIN) / sizeof(BUILTIN[0]));
int         builtinCount()       { return NBUILTIN; }
const char* builtinName(int i)   { return (i >= 0 && i < NBUILTIN) ? BUILTIN[i].name : ""; }
const char* builtinRtttl(int i)  { return (i >= 0 && i < NBUILTIN) ? BUILTIN[i].rtttl : ""; }

// ---- note maths
uint16_t noteFreq(uint8_t midi) {
  if (!midi) return 0;
  return (uint16_t)lroundf(440.0f * powf(2.0f, (midi - 69) / 12.0f));
}
uint32_t noteMs(const Song& s, const SongNote& n) {
  uint32_t whole = 240000UL / (s.bpm ? s.bpm : 120);        // 4 beats of a quarter note
  uint32_t ms = whole / (n.len ? n.len : 4);
  return n.dot ? ms * 3 / 2 : ms;
}
uint32_t songLengthMs(const Song& s) { uint32_t t = 0; for (uint16_t i = 0; i < s.n; i++) t += noteMs(s, s.notes[i]); return t; }

// ---- RTTTL
static void setErr(char* e, size_t n, const char* m) { if (e && n) strlcpy(e, m, n); }
static bool validLen(int d) { return d == 1 || d == 2 || d == 4 || d == 8 || d == 16 || d == 32; }

bool rtttlParse(const char* t, Song& out, char* err, size_t errLen) {
  memset(&out, 0, sizeof(out));
  if (!t) { setErr(err, errLen, "empty"); return false; }
  const char* c1 = strchr(t, ':');
  if (!c1) { setErr(err, errLen, "missing ':' after the name"); return false; }
  size_t nl = min<size_t>(c1 - t, SONG_NAME_MAX);
  memcpy(out.name, t, nl); out.name[nl] = 0;
  while (nl && out.name[nl - 1] == ' ') out.name[--nl] = 0;
  if (!out.name[0]) strcpy(out.name, "Song");
  const char* c2 = strchr(c1 + 1, ':');
  if (!c2) { setErr(err, errLen, "missing ':' before the notes"); return false; }
  int defD = 4, defO = 6, bpm = 63;
  for (const char* p = c1 + 1; p < c2; ) {
    while (p < c2 && (*p == ' ' || *p == ',')) p++;
    if (p >= c2) break;
    char k = tolower(*p); p++;
    while (p < c2 && *p == ' ') p++;
    if (p < c2 && *p == '=') p++;
    int v = atoi(p);
    while (p < c2 && isdigit((unsigned char)*p)) p++;
    if (k == 'd') { if (!validLen(v)) { setErr(err, errLen, "d must be 1,2,4,8,16 or 32"); return false; } defD = v; }
    else if (k == 'o') { if (v < 3 || v > 8) { setErr(err, errLen, "o must be 3..8"); return false; } defO = v; }
    else if (k == 'b') { if (v < 20 || v > 400) { setErr(err, errLen, "b (tempo) must be 20..400"); return false; } bpm = v; }
    else { setErr(err, errLen, "unknown setting (use d, o, b)"); return false; }
  }
  out.bpm = (uint16_t)bpm;
  static const int8_t SEMI[7] = { 9, 11, 0, 2, 4, 5, 7 };     // a b c d e f g
  for (const char* p = c2 + 1; *p; ) {
    while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (!*p) break;
    if (out.n >= SONG_MAX_NOTES) { setErr(err, errLen, "too many notes (max 512)"); return false; }
    int d = 0;
    while (isdigit((unsigned char)*p)) d = d * 10 + (*p++ - '0');
    if (!d) d = defD;
    if (!validLen(d)) { setErr(err, errLen, "note length must be 1,2,4,8,16 or 32"); return false; }
    char nc = tolower(*p);
    SongNote& n = out.notes[out.n];
    n.len = (uint8_t)d;
    if (nc == 'p' || nc == 'r') { n.midi = 0; p++; }
    else if (nc >= 'a' && nc <= 'g') {
      int semi = SEMI[nc - 'a']; p++;
      if (*p == '#') { semi++; p++; }
      else if (*p == '_') { semi--; p++; }                  // flat (some tools)
      if (*p == '.') { n.dot = true; p++; }
      int o = defO;
      if (isdigit((unsigned char)*p)) { o = *p - '0'; p++; }
      if (o < 3 || o > 8) { setErr(err, errLen, "octave must be 3..8"); return false; }
      n.midi = (uint8_t)clampv(12 * (o + 1) + semi, 1, 127);
    } else { char m[40]; snprintf(m, sizeof(m), "unexpected '%c' in the notes", *p ? *p : '?'); setErr(err, errLen, m); return false; }
    if (*p == '.') { n.dot = true; p++; }
    while (*p == ' ') p++;
    if (*p && *p != ',') { char m[40]; snprintf(m, sizeof(m), "expected ',' near '%c'", *p); setErr(err, errLen, m); return false; }
    out.n++;
  }
  if (!out.n) { setErr(err, errLen, "no notes"); return false; }
  return true;
}

size_t rtttlWrite(const Song& s, char* out, size_t cap) {
  static const char* NM[12] = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
  size_t w = snprintf(out, cap, "%s:d=4,o=5,b=%u:", s.name[0] ? s.name : "Song", s.bpm ? s.bpm : 120);
  for (uint16_t i = 0; i < s.n && w < cap; i++) {
    const SongNote& n = s.notes[i];
    char tok[12];
    const char* sep = i ? "," : "";
    char len[4] = ""; if (n.len != 4) snprintf(len, sizeof(len), "%u", n.len);
    if (!n.midi) snprintf(tok, sizeof(tok), "%s%sp%s", sep, len, n.dot ? "." : "");
    else {
      int o = n.midi / 12 - 1; o = clampv(o, 3, 8);
      snprintf(tok, sizeof(tok), "%s%s%s%s%s", sep, len, NM[n.midi % 12], n.dot ? "." : "", o == 5 ? "" : String(o).c_str());
    }
    size_t tl = strlen(tok);
    if (w + tl >= cap) break;
    memcpy(out + w, tok, tl + 1); w += tl;
  }
  return w;
}

// ---- storage
static char s_names[SONG_SLOTS][SONG_NAME_MAX + 1];
static int  s_lastSaved = 0;
static bool loadDoc(JsonDocument& d) {
  String txt;
  if (storeRead(SONG_FILE, txt) && !deserializeJson(d, txt) && d["songs"].is<JsonArray>()) return true;
  d.clear(); d["v"] = 1; d["songs"].to<JsonArray>(); return false;
}
static void cacheNames() {
  JsonDocument d; loadDoc(d);
  JsonArrayConst a = d["songs"];
  for (int i = 0; i < SONG_SLOTS; i++) {
    JsonVariantConst e = a[i];
    strlcpy(s_names[i], e.isNull() ? "" : (const char*)(e["name"] | "Song"), sizeof(s_names[0]));
  }
}
int songCount() { int n = 0; for (int i = 0; i < SONG_SLOTS; i++) if (s_names[i][0]) n++; return n; }
const char* songName(int slot) { return (slot >= 1 && slot <= SONG_SLOTS) ? s_names[slot - 1] : ""; }
int songLastSaved() { return s_lastSaved; }
static bool rtttlOf(int slot, String& out) {
  if (slot < 1 || slot > SONG_SLOTS || !s_names[slot - 1][0]) return false;
  JsonDocument d; loadDoc(d);
  out = (const char*)(d["songs"][slot - 1]["rtttl"] | "");
  return out.length() > 0;
}
bool songLoad(int slot, Song& s) { String r; return rtttlOf(slot, r) && rtttlParse(r.c_str(), s); }
static bool saveRtttl(int slot, const char* name, const char* rtttl) {
  JsonDocument d; loadDoc(d);
  JsonArray a = d["songs"];
  while (a.size() < SONG_SLOTS) a.add(nullptr);
  if (slot == 0) { for (int i = 0; i < SONG_SLOTS; i++) if (a[i].isNull()) { slot = i + 1; break; } }
  if (slot < 1 || slot > SONG_SLOTS) return false;
  JsonObject o = a[slot - 1].to<JsonObject>();
  o["name"] = name; o["rtttl"] = rtttl;
  String txt; serializeJson(d, txt);
  if (!storeWrite(SONG_FILE, txt.c_str(), txt.length())) return false;
  s_lastSaved = slot; cacheNames();
  return true;
}
bool songSave(int slot, const Song& s) {
  static char buf[RTTTL_MAX];
  rtttlWrite(s, buf, sizeof(buf));
  return saveRtttl(slot, s.name, buf);
}
bool songDelete(int slot) {
  if (slot < 1 || slot > SONG_SLOTS) return false;
  JsonDocument d; loadDoc(d);
  JsonArray a = d["songs"];
  while (a.size() < SONG_SLOTS) a.add(nullptr);
  a[slot - 1].set(nullptr);
  String txt; serializeJson(d, txt);
  bool ok = storeWrite(SONG_FILE, txt.c_str(), txt.length());
  cacheNames();
  return ok;
}

// ---- playback
static MusicNote s_buf[SONG_MAX_NOTES * 2];
static Song      s_play;                     // scratch for parsing
static uint16_t  s_from = 0;

void songPlayStructEx(const Song& s, uint8_t flags, uint16_t from) {
  uint16_t k = 0;
  for (uint16_t i = from; i < s.n && k + 2 <= ARRAY_LEN(s_buf); i++) {
    uint32_t ms = noteMs(s, s.notes[i]);
    uint16_t f = noteFreq(s.notes[i].midi);
    if (!f) { s_buf[k++] = { 0, (uint16_t)ms }; continue; }
    uint16_t gap = (uint16_t)max<uint32_t>(8, ms / 9);       // a little air between notes
    s_buf[k++] = { f, (uint16_t)(ms - gap) };
    s_buf[k++] = { 0, gap };
  }
  s_from = from;
  musicPlayEx(s_buf, k, flags);
}
void songPlayStruct(const Song& s, bool loop, uint16_t from) { songPlayStructEx(s, loop ? MUS_LOOP : 0, from); }
bool songPlaySlot(int slot, bool loop, bool loud) {
  if (!songLoad(slot, s_play)) return false;
  songPlayStructEx(s_play, (loop ? MUS_LOOP : 0) | (loud ? MUS_LOUD : 0), 0);
  return true;
}
bool songPlayRtttl(const char* r, bool loop, bool game) {
  if (!rtttlParse(r, s_play)) return false;
  songPlayStructEx(s_play, (loop ? MUS_LOOP : 0) | (game ? MUS_GAME : 0), 0);
  return true;
}
void     songStop()     { musicStop(); }
bool     songPlaying()  { return musicPlaying(); }
uint16_t songPosition() { return s_from + musicIndex() / 2; }

bool tonePlay(ToneKind k, bool loop) {
  if (k > TONE_BOOT) return false;                             // 255 = siren only (find-me)
  uint8_t slot = k == TONE_ALARM ? S.songAlarm : k == TONE_TIMER ? S.songTimer : k == TONE_NOTIFY ? S.songNotify : S.songBoot;
  if (!slot) return false;
  if (slot > SONG_SLOTS) {                                     // 17.. = built-in tunes
    int b = slot - SONG_SLOTS - 1;
    if (b < 0 || b >= NBUILTIN || !rtttlParse(BUILTIN[b].rtttl, s_play)) return false;
    songPlayStructEx(s_play, (loop ? MUS_LOOP : 0) | (k <= TONE_TIMER ? MUS_LOUD : 0), 0);
    return true;
  }
  return songPlaySlot(slot, loop, k <= TONE_TIMER);
}

// ---- protocol
static bool cSongList(JsonObjectConst, JsonObject r, ProtoCtx&) {
  JsonArray a = r["songs"].to<JsonArray>();
  JsonDocument d; loadDoc(d);
  for (int i = 0; i < SONG_SLOTS; i++) {
    if (!s_names[i][0]) continue;
    JsonObject o = a.add<JsonObject>(); o["slot"] = i + 1; o["name"] = s_names[i];
    o["rtttl"] = d["songs"][i]["rtttl"] | "";
  }
  JsonArray b = r["builtin"].to<JsonArray>();
  for (int i = 0; i < NBUILTIN; i++) { JsonObject o = b.add<JsonObject>(); o["slot"] = SONG_SLOTS + 1 + i; o["name"] = BUILTIN[i].name; o["rtttl"] = BUILTIN[i].rtttl; }
  r["max"] = SONG_SLOTS; r["maxNotes"] = SONG_MAX_NOTES;
  return true;
}
static bool cSongSet(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  const char* rt = q["rtttl"] | "";
  char err[48];
  if (strlen(rt) >= RTTTL_MAX) { r["err"] = "song too long"; return false; }
  if (!rtttlParse(rt, s_play, err, sizeof(err))) { r["err"] = err; return false; }
  int slot = q["slot"] | 0;
  if (!saveRtttl(slot, s_play.name, rt)) { r["err"] = slot ? "write failed" : "all 16 song slots are full"; return false; }
  r["slot"] = s_lastSaved; r["notes"] = s_play.n; r["ms"] = songLengthMs(s_play);
  return true;
}
static bool cSongDel(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  if (!songDelete(q["slot"] | 0)) { r["err"] = "no such song"; return false; }
  return true;
}
static bool cSongPlay(JsonObjectConst q, JsonObject r, ProtoCtx&) {
  if (q["stop"] | false) { songStop(); return true; }
  const char* rt = q["rtttl"] | "";
  bool loop = q["loop"] | false;
  char err[48];
  if (rt[0]) {
    if (!rtttlParse(rt, s_play, err, sizeof(err))) { r["err"] = err; return false; }
    songPlayStructEx(s_play, loop ? MUS_LOOP : 0, (uint16_t)(q["from"] | 0));
    return true;
  }
  int slot = q["slot"] | 0;
  if (slot > SONG_SLOTS && slot <= SONG_SLOTS + NBUILTIN) return songPlayRtttl(BUILTIN[slot - SONG_SLOTS - 1].rtttl, loop);
  if (!songPlaySlot(slot, loop)) { r["err"] = "no such song"; return false; }
  return true;
}
static bool cToneTest(JsonObjectConst q, JsonObject, ProtoCtx&) {
  int hz = q["hz"] | 0;
  if (hz <= 0) buzzAlarmStop(); else { buzzTestTone((uint16_t)clampv(hz, 200, 8000)); }
  return true;
}

void musicLibBegin() {
  cacheNames();
  protoRegister("song.list", cSongList);
  protoRegister("song.set",  cSongSet);
  protoRegister("song.del",  cSongDel);
  protoRegister("song.play", cSongPlay);
  protoRegister("tone.test", cToneTest);
}
