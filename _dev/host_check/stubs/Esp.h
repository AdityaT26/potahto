#pragma once
#include <stdint.h>
class EspClass { public: uint32_t getFreeHeap() { return 201000; } uint32_t getPsramSize() { return 8 * 1048576; } void restart() {} };
extern EspClass ESP;
