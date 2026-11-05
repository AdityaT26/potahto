#pragma once
#include <Arduino.h>
typedef const uint8_t* u8g2_font_t_ptr;
extern const uint8_t u8g2_font_4x6_tf[], u8g2_font_5x7_tf[], u8g2_font_6x12_tf[],
  u8g2_font_7x13B_tf[], u8g2_font_logisoso24_tr[], u8g2_font_logisoso28_tn[],
  u8g2_font_6x10_tf[], u8g2_font_5x8_tf[], u8g2_font_open_iconic_all_1x_t[];
struct u8g2_cb_t { int r; };
extern const u8g2_cb_t* U8G2_R0; extern const u8g2_cb_t* U8G2_R1;
extern const u8g2_cb_t* U8G2_R2; extern const u8g2_cb_t* U8G2_R3;
#define U8X8_PIN_NONE 255
#define U8G2_DRAW_UPPER_RIGHT 0x01
#define U8G2_DRAW_UPPER_LEFT 0x02
#define U8G2_DRAW_LOWER_LEFT 0x04
#define U8G2_DRAW_LOWER_RIGHT 0x08
#define U8G2_DRAW_ALL 0x0F
class U8G2 : public Print {
 protected:
  uint8_t buf[1024]; const u8g2_cb_t* rot; int fw = 6;
 public:
  U8G2() : rot(nullptr) { memset(buf, 0, sizeof buf); }
  bool begin() { return true; }
  void clearBuffer() { memset(buf, 0, sizeof buf); }
  void sendBuffer() {}
  void clear() {}
  void setPowerSave(uint8_t) {}
  void setContrast(uint8_t) {}
  void setFlipMode(uint8_t) {}
  uint8_t* getBufferPtr() { return buf; }
  uint8_t getBufferTileWidth() { return 16; }
  uint8_t getBufferTileHeight() { return 8; }
  void updateDisplayArea(uint8_t, uint8_t, uint8_t, uint8_t) {}
  void updateDisplay() {}
  void setDisplayRotation(const u8g2_cb_t* r) { rot = r; }
  int getDisplayWidth() { return (rot == U8G2_R1 || rot == U8G2_R3) ? 64 : 128; }
  int getDisplayHeight() { return (rot == U8G2_R1 || rot == U8G2_R3) ? 128 : 64; }
  void setFont(const uint8_t*) {}
  void setFontMode(uint8_t) {}
  void setFontPosBaseline() {} void setFontPosTop() {} void setFontPosCenter() {}
  void setDrawColor(uint8_t) {}
  void setBitmapMode(uint8_t) {}
  int getStrWidth(const char* s) { return (int)strlen(s) * fw; }
  int getUTF8Width(const char* s) { return getStrWidth(s); }
  int8_t getAscent() { return 8; } int8_t getDescent() { return -2; }
  int8_t getMaxCharHeight() { return 10; } int8_t getMaxCharWidth() { return 6; }
  int drawStr(int, int, const char* s) { return getStrWidth(s); }
  int drawUTF8(int, int, const char* s) { return getStrWidth(s); }
  int drawGlyph(int, int, uint16_t) { return 6; }
  void drawPixel(int, int) {}
  void drawLine(int, int, int, int) {}
  void drawHLine(int, int, int) {}
  void drawVLine(int, int, int) {}
  void drawBox(int, int, int, int) {}
  void drawFrame(int, int, int, int) {}
  void drawRBox(int, int, int, int, int) {}
  void drawRFrame(int, int, int, int, int) {}
  void drawCircle(int, int, int, uint8_t = 0xF) {}
  void drawDisc(int, int, int, uint8_t = 0xF) {}
  void drawEllipse(int, int, int, int, uint8_t = 0xF) {}
  void drawFilledEllipse(int, int, int, int, uint8_t = 0xF) {}
  void drawTriangle(int, int, int, int, int, int) {}
  void drawXBM(int, int, int, int, const uint8_t*) {}
  void drawXBMP(int, int, int, int, const uint8_t*) {}
  void setCursor(int, int) {}
  void setClipWindow(int, int, int, int) {}
  void setMaxClipWindow() {}
  size_t write(uint8_t) override { return 1; }
  using Print::write;
};
class U8G2_SSD1306_128X64_NONAME_F_HW_I2C : public U8G2 {
 public:
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C(const u8g2_cb_t* r, uint8_t = U8X8_PIN_NONE, uint8_t = U8X8_PIN_NONE, uint8_t = U8X8_PIN_NONE) { rot = r; }
};
