// Songs for the buzzer: RTTTL, a song library, tones for events
//
// Songs are RTTTL ("Nokia ringtone") text, so you can paste thousands of
// existing tunes, write your own in the web app's Composer (piano roll) or on
// the device (Music > Compose). Up to 16 songs of up to 512 notes live in
// /songs.json (LittleFS, survives updates).
//   name:d=4,o=5,b=140:8c6,8e6,g6,p,2c7.
//   d = default length (1 2 4 8 16 32), o = default octave (3..8), b = BPM
//   note = [length] a..g or p (rest) [#] [.] [octave] [.]
// Songs can be the alarm / timer / message / boot tone (Settings > Sound).
// Each game has its own built-in theme.
#pragma once
#include <Arduino.h>

#define SONG_SLOTS     16
#define SONG_MAX_NOTES 512
#define SONG_NAME_MAX  23

struct SongNote { uint8_t midi; uint8_t len; bool dot; };      // midi 0 = rest, len 1..32
struct Song {
  char     name[SONG_NAME_MAX + 1];
  uint16_t bpm;
  uint16_t n;
  SongNote notes[SONG_MAX_NOTES];
};

void        musicLibBegin();                 // load names, protocol commands
int         songCount();                     // user songs (slots 1..n)
const char* songName(int slot);              // slot 1..16 ("" if empty)
bool        songLoad(int slot, Song& out);   // parse a stored song
bool        songSave(int slot, const Song& s);   // slot 0 = append; returns false if full
bool        songDelete(int slot);
int         songLastSaved();                 // slot of the last successful save

bool        rtttlParse(const char* txt, Song& out, char* err = nullptr, size_t errLen = 0);
size_t      rtttlWrite(const Song& s, char* out, size_t cap);
uint16_t    noteFreq(uint8_t midi);          // 0 for rests
uint32_t    noteMs(const Song& s, const SongNote& n);
uint32_t    songLengthMs(const Song& s);

// playback (owns its buffer; plays through the feedback engine's music channel)
bool        songPlaySlot(int slot, bool loop, bool loud = false);   // 0 / missing -> false
bool        songPlayRtttl(const char* rtttl, bool loop, bool game = false);
void        songPlayStruct(const Song& s, bool loop, uint16_t fromNote = 0);
void        songStop();
bool        songPlaying();
uint16_t    songPosition();                  // index of the note playing now

// event tones: plays the song chosen in Settings > Sound, or the built-in one
enum ToneKind : uint8_t { TONE_ALARM = 0, TONE_TIMER, TONE_NOTIFY, TONE_BOOT };
bool        tonePlay(ToneKind k, bool loop);  // true = a user song is playing

// built-in songs (read-only, shown in Music after your own)
int         builtinCount();
const char* builtinName(int i);
const char* builtinRtttl(int i);
