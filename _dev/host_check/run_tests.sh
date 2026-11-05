#!/bin/bash
# Unit tests that run on the PC: macro engine, settings persistence, battery smoothing.
#   bash _dev/host_check/run_tests.sh
set -e
H=$(cd "$(dirname "$0")" && pwd); SRC=$(cd "$H/../.." && pwd); OUT=${TMPDIR:-/tmp}/potahto_tests
mkdir -p "$OUT"
F="-std=gnu++17 -w -I$H/stubs -I$SRC -include $H/stubs/Arduino.h"
g++ $F "$H/tests/test_macro.cpp" "$SRC/HidMacro.cpp" "$H/stubs/stubs.cpp" -o "$OUT/test_macro"
g++ $F "$H/tests/test_settings.cpp" "$SRC/Settings.cpp" "$SRC/Globals.cpp" "$SRC/Sensors.cpp" "$H/stubs/stubs.cpp" "$H/tests/fake_tof.cpp" -o "$OUT/test_settings"
g++ $F "$H/tests/test_batt.cpp" "$SRC/Settings.cpp" "$SRC/Globals.cpp" "$SRC/Sensors.cpp" "$H/stubs/stubs.cpp" "$H/tests/fake_tof.cpp" -o "$OUT/test_batt"
(cd "$H/tests" && "$OUT/test_macro")
"$OUT/test_settings"
"$OUT/test_batt"
