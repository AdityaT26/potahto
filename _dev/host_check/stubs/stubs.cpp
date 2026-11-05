#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <U8g2lib.h>
uint32_t g_fakeMillis = 1000;
uint32_t g_fakeAdcMv = 1900;
uint8_t g_pinLevel[64];
static struct PinInit { PinInit() { memset(g_pinLevel, 1, sizeof g_pinLevel); } } s_pinInit;
HWCDC Serial;
TwoWire Wire;
std::map<std::string, std::map<std::string, NvsVal>> g_nvs;
int g_nvsWrites = 0;
const uint8_t u8g2_font_4x6_tf[1]={0}, u8g2_font_5x7_tf[1]={0}, u8g2_font_6x12_tf[1]={0},
  u8g2_font_7x13B_tf[1]={0}, u8g2_font_logisoso24_tr[1]={0}, u8g2_font_logisoso28_tn[1]={0},
  u8g2_font_6x10_tf[1]={0}, u8g2_font_5x8_tf[1]={0}, u8g2_font_open_iconic_all_1x_t[1]={0};
static const u8g2_cb_t r0{0}, r1{1}, r2{2}, r3{3};
const u8g2_cb_t* U8G2_R0 = &r0; const u8g2_cb_t* U8G2_R1 = &r1;
const u8g2_cb_t* U8G2_R2 = &r2; const u8g2_cb_t* U8G2_R3 = &r3;
