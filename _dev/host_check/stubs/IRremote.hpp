#pragma once
#include <Arduino.h>
enum decode_type_t { UNKNOWN = 0, PULSE_WIDTH, PULSE_DISTANCE, NEC, SONY, RC5, RC6, SAMSUNG };
#define IRDATA_FLAGS_EMPTY 0x00
#define IRDATA_FLAGS_IS_REPEAT 0x01
#define IRDATA_FLAGS_IS_AUTO_REPEAT 0x02
#define IRDATA_FLAGS_PARITY_FAILED 0x04
#define IRDATA_FLAGS_WAS_OVERFLOW 0x40
#define DISABLE_LED_FEEDBACK false
struct IRData {
  decode_type_t protocol; uint16_t address; uint16_t command; uint16_t extra;
  uint16_t numberOfBits; uint8_t flags; uint64_t decodedRawData; uint16_t rawlen;
};
class IRrecv {
 public:
  IRData decodedIRData{};
  void begin(uint8_t, bool = false) {} void start() {} void stop() {} void resume() {}
  bool decode() { return false; }
  void compensateAndStoreIRResultInArray(uint8_t*) {}
};
class IRsend {
 public:
  void begin(uint8_t) {}
  void sendRaw(const uint8_t*, uint16_t, uint8_t) {}
  void sendRaw(const uint16_t*, uint16_t, uint8_t) {}
  size_t write(IRData*, int_fast8_t = 0) { return 1; }
};
inline IRrecv IrReceiver; inline IRsend IrSender;
