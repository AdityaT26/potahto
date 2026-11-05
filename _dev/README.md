# _dev

Tools I used to test Potahto without flashing it. None of this is part of the
firmware build (the Arduino IDE ignores this folder). Linux or WSL, with g++
and Python 3. Point `ARDUINOJSON` and `U8G2` at your Arduino library folders
(default `~/Arduino/libraries/...`).

| What | How |
|---|---|
| Compile check | `bash _dev/host_check/build.sh .` compiles the firmware with g++ against stub headers. |
| Unit tests | `bash _dev/host_check/run_tests.sh` runs the macro engine (every shipped macro must parse), settings persistence and battery smoothing tests. |
| Screen simulator | `bash _dev/oled/build_oled.sh` builds the real `setup()`/`loop()` and UI for the PC. `cd _dev/oled && ./oled_sim scripts/settings_tour.script` presses buttons and writes frames to `shots/`; `python3 sheet.py out.png $(ls shots \| sed 's/.pbm//')` turns them into a contact sheet. |
| Web app tests | `bash _dev/sim/build_sim.sh`, then `bash _dev/sim/restart_server.sh` and `python3 _dev/sim/ui_test.py` (Playwright) drives every page of the web app against the simulated device, in light, dark and phone width. |
| `embed_webapp.py` | Regenerates `WebApp.h` from `webapp/index.html`. Run it after editing the web app or the companion. |
| `ctags_wrapper.py` | Only needed if you build with arduino-cli and exuberant-ctags. |
