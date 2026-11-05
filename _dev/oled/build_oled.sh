#!/bin/bash
# Builds the "virtual Potahto": the real setup()/loop(), UI and U8g2 compiled
# for the PC, with fake sensors and radios. Output: _dev/oled/oled_sim
#
#   U8G2=~/Arduino/libraries/U8g2 ARDUINOJSON=~/Arduino/libraries/ArduinoJson bash _dev/oled/build_oled.sh
#   cd _dev/oled && ./oled_sim scripts/settings_tour.script && python3 sheet.py out.png $(ls shots | sed 's/.pbm//')
set -e
O=$(cd "$(dirname "$0")" && pwd); SRC=$(cd "$O/../.." && pwd); STUBS=$O/../host_check/stubs
U8G2=${U8G2:-$HOME/Arduino/libraries/U8g2}; AJ=${ARDUINOJSON:-$HOME/Arduino/libraries/ArduinoJson}
mkdir -p "$O/fw" "$O/shots"; rm -f "$O"/fw/*.o
if [ ! -f "$O/fw/libu8g2host.a" ]; then          # U8g2's C core, built once
  mkdir -p "$O/fw/u8g2"; for c in "$U8G2"/src/clib/*.c; do gcc -O1 -c "$c" -I"$U8G2/src/clib" -o "$O/fw/u8g2/$(basename "$c" .c).o"; done
  for c in U8g2lib U8x8lib; do g++ -std=gnu++17 -O1 -w -DU8X8_NO_HW_SPI -DU8X8_NO_HW_I2C -I"$O/inc" -I"$U8G2/src" -I"$STUBS" -include "$STUBS/Arduino.h" -c "$U8G2/src/$c.cpp" -o "$O/fw/u8g2/$c.o"; done
  ar rcs "$O/fw/libu8g2host.a" "$O"/fw/u8g2/*.o
fi
F="-std=gnu++17 -O0 -w -DPOTAHTO_SIM -DU8X8_NO_HW_SPI -DU8X8_NO_HW_I2C -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -DARDUINOJSON_ENABLE_PROGMEM=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0"
F="$F -I$AJ/src -I$O/inc -I$U8G2/src -I$STUBS -I$SRC -include $STUBS/Arduino.h"
# radio / filesystem / web files are faked in oled_glue.cpp
for f in "$SRC"/*.cpp; do
  case $(basename "$f") in Net.cpp|WebSrv.cpp|Proto.cpp|Hid.cpp|Store.cpp|Iot.cpp|Wireless.cpp|BleLink.cpp) continue;; esac
  g++ $F -c "$f" -o "$O/fw/$(basename "$f").o"
done
gcc -c -O1 "$SRC/PotQR.c" -o "$O/fw/PotQR.o"
g++ $F -x c++ -c "$SRC/potahto.ino" -o "$O/fw/ino.o"
g++ $F -c "$O/oled_glue.cpp" -o "$O/fw/glue.o"
g++ "$O"/fw/*.o "$O/fw/libu8g2host.a" -o "$O/oled_sim"
echo "built $O/oled_sim"
