// Non-blocking haptics / audio / light engine
//
// Nothing here ever blocks.  Call fbUpdate() once per loop and it advances any
// running buzzer sequence, motor pulse and neopixel animation.  All three
// respect the master enable/strength switches in Settings, so features can just
// fire semantic events (fbSelect(), fbError(), ...) and stay clean.
#pragma once
#include <Arduino.h>

struct ToneStep { uint16_t freq; uint16_t dur; };   // freq==0 -> silent rest

enum NpxMode : uint8_t {
  NPX_OFF = 0, NPX_SOLID, NPX_BREATHE, NPX_BLINK, NPX_RAINBOW, NPX_PULSE
};

void fbBegin();
void fbUpdate();          // call every loop

// ---- background music (monophonic; auto-ducks under buzzer SFX)
struct MusicNote { uint16_t freq; uint16_t dur; };   // freq 0 = rest
enum : uint8_t { MUS_LOOP = 1, MUS_GAME = 2, MUS_LOUD = 4 };
// MUS_GAME : gated by Games > Music, a little quieter
// MUS_LOUD : alarms - full drive, plays even when the buzzer is muted
void musicPlay(const MusicNote* melody, uint16_t count);  // game theme (loop + game gate)
void musicPlayEx(const MusicNote* melody, uint16_t count, uint8_t flags);
void musicStop();
bool musicPlaying();
uint16_t musicIndex();                                     // entry playing now

// ---- buzzer
void buzzTone(uint16_t freq, uint16_t durMs);                 // master volume
void buzzToneVol(uint16_t freq, uint16_t durMs, uint8_t vol); // vol 0..10
void buzzSeq(const ToneStep* steps, uint8_t count);           // async jingle
void buzzStop();
// Continuous alarm tone: bypasses the global buzzer-enable master (an armed
// alarm should still sound even if UI buzzer is off) and holds the tone with
// no gaps until buzzAlarmStop(). Re-call each frame to keep it asserted even
// if a short UI SFX momentarily grabs the buzzer. vol is 0..10 (local).
void buzzAlarmTone(uint16_t freq, uint8_t vol);
void buzzAlarmStop();
// Loud siren: call every frame while it should sound. Sweeps through the
// buzzer's resonant frequency (S.buzResHz) at full drive; vol 0..10.
void buzzSiren(uint8_t vol);
// Loudness test: hold one frequency at full drive (Settings > Sound).
void buzzTestTone(uint16_t freq);

// ---- motor
void motorPulse(uint16_t ms);                 // master strength
void motorPulseStr(uint16_t ms, uint8_t str); // str 0..10
void motorPattern(const uint16_t* onOff, uint8_t count); // on,off,on,off... ms
void motorOff();

// ---- neopixel
uint32_t npxHSV(uint8_t hue, uint8_t sat, uint8_t val);
void npxSetBase(NpxMode mode, uint32_t color, uint8_t speed);  // persistent
void npxSolid(uint32_t color);
void npxOff();
void npxFlash(uint32_t color, uint8_t times, uint16_t periodMs); // transient
void npxForceRefresh();   // re-apply after brightness/enable change
void npxKeepOn();         // call each frame to keep the LED lit through the idle timeout
void npxKeepOnBright(uint8_t b);   // same, at its own brightness (torch)

// ---- semantic events (used everywhere)
void fbNav();       // move highlight
void fbEdge();      // hit end of a list
void fbSelect();    // open / confirm-ish
void fbBack();      // leave a screen
void fbConfirm();   // positive result (saved, done)
void fbCancel();    // dismissed
void fbError();     // invalid / failed
void fbToastCue();  // little blip when a toast pops
void fbBoot();      // startup jingle + light sweep
