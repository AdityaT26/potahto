// Background engines for the Desk apps
//
// Alarms (3, weekly days or one-shot), a countdown timer and the Pomodoro
// cycle keep running while you use other apps; they ring / chime and emit
// smart-home events. PC stats arrive from the companion script ("pc.stats").
#pragma once
#include <Arduino.h>

void deskBegin();                 // protocol commands
void deskLoop();                  // once per frame

// timer
void     timerStart(uint16_t minutes);
void     timerStop();
bool     timerRunning();
uint32_t timerLeftMs();

// alarms
int      alarmNextIndex(int* minutesUntil);   // -1 if none enabled / no time

// pomodoro
enum PomPhase : uint8_t { POM_IDLE = 0, POM_FOCUS, POM_SHORT, POM_LONG };
PomPhase pomPhase();
bool     pomRunning();
uint32_t pomLeftMs();
uint32_t pomPhaseMs();
uint8_t  pomRound();               // 1..rounds within the current set
uint16_t pomDoneToday();
void     pomToggle();              // start / pause
void     pomSkip();
void     pomReset();

// PC monitor
struct PcStats {
  float cpu, ram, gpu, cpuT, gpuT, upKBs, downKBs, disk;
  char  host[24];
  uint32_t at;                     // millis() of last update (0 = never)
  uint8_t hist[64]; uint8_t histN; // CPU % history, oldest first
};
const PcStats& pcStats();
