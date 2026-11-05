#!/bin/bash
# Device simulator for the web app: the real protocol, settings, macro, music
# and time code compiled for the PC, fake hardware. Output: _dev/sim/potahto_sim
#   ARDUINOJSON=~/Arduino/libraries/ArduinoJson bash _dev/sim/build_sim.sh
set -e
S=$(cd "$(dirname "$0")" && pwd); SRC=$(cd "$S/../.." && pwd); STUBS=$S/../host_check/stubs
AJ=${ARDUINOJSON:-$HOME/Arduino/libraries/ArduinoJson}
gcc -c "$SRC/PotQR.c" -o "$S/potqr.o"
g++ -std=gnu++17 -w -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -DARDUINOJSON_ENABLE_PROGMEM=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 \
  -I"$STUBS" -I"$SRC" -I"$AJ/src" -include "$STUBS/Arduino.h" \
  "$S/sim_glue.cpp" "$SRC"/{Proto,Macros,HidMacro,Iot,Notify,Desk,Feedback,Input,UI,Settings,TimeSvc,Music,Globals}.cpp \
  "$STUBS/idf_host.cpp" "$STUBS/stubs.cpp" "$S/potqr.o" -lpthread -o "$S/potahto_sim"
echo "built $S/potahto_sim"
