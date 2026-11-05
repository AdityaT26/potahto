#!/bin/bash
# Host syntax/semantics check of the firmware against stub headers.
# usage: build.sh <src_dir>   (exits non-zero on any compile error)
SRC=${1:-.}
OUT=${TMPDIR:-/tmp}/potahto_hostcheck; rm -rf $OUT; mkdir -p $OUT
H=$(cd "$(dirname "$0")" && pwd)
AJ=${ARDUINOJSON:-$HOME/Arduino/libraries/ArduinoJson}/src
FLAGS="-std=gnu++17 -c -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -DARDUINOJSON_ENABLE_PROGMEM=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -I$H/stubs -I$SRC -I$AJ -include $H/stubs/Arduino.h"
fail=0
cp $SRC/*.ino $OUT/main.cpp 2>/dev/null
# radio / filesystem / web-server files need the real ESP32 core: the real build covers them
SKIP="BleLink.cpp Hid.cpp Net.cpp Store.cpp WebSrv.cpp Wireless.cpp"
for f in $SRC/*.cpp $OUT/main.cpp $H/stubs/stubs.cpp; do
  case " $SKIP " in *" $(basename $f) "*) continue;; esac
  g++ $FLAGS "$f" -o $OUT/$(basename "$f").o 2> $OUT/$(basename "$f").log || { echo "FAIL $(basename $f)"; head -30 $OUT/$(basename "$f").log; fail=1; }
  grep -h "warning" $OUT/$(basename "$f").log | head -5
done
[ $fail = 0 ] && echo "ALL TUs COMPILED"
exit $fail
