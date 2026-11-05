# Host compile check (not part of the firmware)

Arduino IDE ignores this folder (only `src/` subfolders are compiled), so it is safe here.

Compiles every firmware `.cpp` with plain `g++` against stub headers in `stubs/`
to catch syntax / API errors without an ESP32 toolchain, and runs unit tests
(`tests/`).

    ARDUINOJSON=~/Arduino/libraries/ArduinoJson bash _dev/host_check/build.sh .   # compile check (Linux/WSL)
    bash _dev/host_check/run_tests.sh                                              # unit tests

Radio, filesystem and web-server files (BleLink, Hid, Net, Store, WebSrv,
Wireless) are skipped: only the real ESP32 build can check those.
Unit tests in `tests/`: macro engine (every shipped macro and library macro
must parse), settings persistence, battery smoothing.

It is NOT a substitute for a real Arduino build: stubs only model the APIs this
firmware uses.
