# Build guide

How to build your own Potahto, from parts to a working device. Nothing here needs a custom PCB. Mine is hand-wired with small breakout boards, so the layout and enclosure are up to you.

Take it one module at a time and test as you go. The boot screen runs a self-test that lists every sensor it finds, so you know straight away if a solder joint is bad.

## Parts

| Qty | Part | Notes |
|-----|------|-------|
| 1 | Seeed XIAO ESP32-S3 | Has USB-C, battery charging and BAT pads on the back. See the pin note below before you buy. |
| 1 | SSD1306 OLED, 128x64, I2C | 0.96" or 1.3" (1.3" ones are often SH1106; swap the `U8G2_SSD1306_...` constructor in `Globals.cpp` for the matching `U8G2_SH1106_...` one) |
| 1 | MPU6050 breakout (GY-521) | accelerometer + gyro |
| 1 | HMC5883L or QMC5883L breakout (GY-271) | compass, either chip works |
| 1 | VL53L0X breakout | time-of-flight distance, up to about 1.2 m |
| 1 | 5-way navigation switch | up/down/left/right/press, common pin to GND |
| 1 | Passive buzzer | passive, not active (it has to play notes) |
| 1 | Coin vibration motor | 3 V type |
| 1 | S8550 PNP transistor | motor driver |
| 1 | 1N4148 diode | across the motor |
| 1 | WS2812B LED | one pixel, a breakout or a bare 5050 |
| 1 | IR LED, 940 nm | e.g. TSAL6100 |
| 1 | 38 kHz IR receiver | e.g. TSOP38238 or VS1838B |
| 1 | Li-ion / LiPo cell, 3.7 V | whatever fits; I use a small one, which is why the power management is so aggressive |
| 2 | 100k resistors | battery voltage divider |
| 1 | 1k resistor | motor transistor base |
| 1 | 68 ohm resistor | IR LED (direct drive) |
| 1 | 330 ohm resistor | WS2812 data line |
| | Thin wire, heat shrink, an enclosure | |

Optional but recommended: a slide switch in the battery line plus the transistor drivers for the buzzer and IR LED described at the end.

## Pin map

| Function | GPIO | XIAO pin |
|----------|------|----------|
| Battery voltage (divider) | 1 | D0 / A0 |
| DOWN | 2 | D1 |
| LEFT | 3 | D2 |
| UP | 4 | D3 |
| I2C SDA | 5 | D4 |
| I2C SCL | 6 | D5 |
| RIGHT | 43 | D6 |
| SELECT | 44 | D7 |
| Motor | 7 | D8 |
| IR receiver | 8 | D9 |
| IR LED | 9 | D10 |
| WS2812 | 12 | back of the board, see below |
| Buzzer | 16 | back of the board, see below |

**About GPIO12 and GPIO16.** The 14 edge pins are all used, so the LED and buzzer sit on pins that aren't on the edge. On a plain XIAO ESP32-S3 these come out through the small board-to-board connector on the underside (the one the Sense camera board plugs into), which takes fine soldering. On the XIAO ESP32-S3 Plus, GPIO12 is the D18 pad on the back. If that's a hassle, move them: the JTAG pads on the back (GPIO39 to GPIO42) work fine. Change `PIN_NEOPIXEL` and `PIN_BUZZER` in `Config.h` to match.

Other things worth knowing:
- GPIO43/44 are the UART pins, but the serial console runs over native USB, so they're free for buttons.
- GPIO3 is a strapping pin. Don't hold LEFT while powering up.
- Everything runs from the XIAO's 3V3 pin. Don't feed 5 V into any sensor's I/O.

## Wiring

### I2C bus (OLED, MPU6050, compass, VL53L0X)

All four share SDA (D4) and SCL (D5), plus 3V3 and GND. Most breakouts already have pull-up resistors, so you don't need to add any. If you have more than three boards with pull-ups and the bus gets flaky, remove a couple of them.

Addresses: OLED `0x3C`, MPU6050 `0x68`, compass `0x1E` (HMC) or `0x0D` (QMC), VL53L0X `0x29`. The firmware probes all of them at boot. Any sensor that's missing just disables the apps that need it.

### Buttons

Wire each direction of the 5-way switch to its GPIO and the common pin to GND. The firmware uses the internal pull-ups, so no resistors are needed. If your switch goes to 3V3 instead, set `BTN_ACTIVE_LOW 0` in `Config.h`.

### Vibration motor

A PNP high-side switch, so the motor is on when the pin is LOW:

```
3V3 ── S8550 emitter
GPIO7 ── 1k ── S8550 base
S8550 collector ── motor + ──┬── 1N4148 cathode
GND ── motor − ──────────────┴── 1N4148 anode
```

The emitter has to go to 3V3, not the battery. Otherwise a HIGH pin (3.3 V) can't switch the transistor fully off. If you use an NPN or a MOSFET low-side instead, set `MOTOR_ACTIVE_LOW 0`.

### Buzzer

Passive buzzer between GPIO16 and GND. That's the simplest version. It works, but it isn't loud; see the transistor driver below.

### WS2812 LED

5V/VDD to 3V3, GND to GND, DIN through the 330 ohm resistor to GPIO12. A 100 nF capacitor across VDD/GND right at the LED helps if it flickers.

### IR

- **Receiver:** VCC to 3V3, GND, OUT to GPIO8.
- **LED:** GPIO9 to a 68 ohm resistor, then the LED anode, with the cathode to GND. That gives about 30 mA, which is what a pin can safely do. For real range, use the driver below.

### Battery

Solder the cell to the BAT+ / BAT− pads on the back of the XIAO. It charges whenever USB is plugged in.

The voltage divider: BAT+ to 100k to GPIO1, then GPIO1 to 100k to GND. That halves the battery voltage so the ADC can read it. The default divider setting (`2.00`) assumes exactly this. You can trim it later in Settings.

If you add a power switch, put it in the BAT+ wire.

## Assembly order

This is the order that lets you test each step:

1. **XIAO alone.** Flash the firmware (see below) with nothing attached. It should enumerate over USB.
2. **OLED.** Wire the I2C bus and the display. On boot you should see the animation and the self-test list.
3. **Buttons.** You can now navigate the menu.
4. **The other I2C sensors.** Add them one at a time. Each should appear as OK in the boot self-test.
5. **Motor, buzzer, LED.** Settings > Sound & buzz has toggles to test each one. Every button press gives feedback too.
6. **IR.** Tools > IR Remote. Record a button from any remote and play it back.
7. **Battery and divider.** Check System > Battery against a multimeter.
8. **Close it up.** Leave the USB-C port and the BOOT button reachable.

## Flashing

See [Flashing the firmware](../README.md#flashing-the-firmware) in the README for the board settings and libraries. In short: XIAO_ESP32S3, USB Mode USB-OTG (TinyUSB), USB CDC On Boot enabled, partition scheme "Default with spiffs (3MB APP/1.5MB SPIFFS)".

If the upload can't find the port, hold BOOT while plugging in the USB cable, upload, then press RESET. After the first flash you can update over WiFi from the web app.

## First boot

1. Go through the tuning list in the README: battery divider, gyro zero, level, buzzer loudest tone, compass.
2. Join your WiFi (Settings > WiFi > Join a network) and pair Bluetooth (Settings > Bluetooth > Pair a new device).
3. Open the web app (`http://potahto.local`, PIN in Settings > Device) and set up macros, alarms and the rest from there.

## Making it louder and longer-range

A GPIO pin gives a few tens of milliamps at 3.3 V. That's the ceiling for both the buzzer and the IR LED. The fix is the same for both: let the pin switch a transistor that runs from the battery.

**Buzzer:**

```
GPIO16 ── 1k ── NPN base (S8050 / 2N2222)
NPN emitter ── GND
VBAT ── buzzer ── NPN collector     (1N4148 across the buzzer if it's a magnetic one)
```

Run Settings > Sound & buzz > Find loudest tone again afterwards.

**IR LED:**

```
GPIO9 ── 1k ── NPN base (or the gate of an AO3400 MOSFET)
NPN emitter ── GND
VBAT ── 10 to 15 ohm ── IR LED anode, LED cathode ── NPN collector
```

That pulses the LED at 100 to 200 mA, which takes the range from a couple of metres to across a room. The firmware doesn't need any changes for either mod.

## Troubleshooting

- **Blank screen:** check SDA/SCL aren't swapped. Some OLEDs are set to `0x3D` instead of `0x3C`; most have a solder jumper on the back to change it.
- **A sensor shows as missing:** usually a cold joint on SDA or SCL, or a breakout running at 5 V logic.
- **Motor always on:** the PNP emitter is on the battery instead of 3V3, or `MOTOR_ACTIVE_LOW` doesn't match your driver.
- **Can't upload:** hold BOOT while plugging in. Close the serial monitor and the web app's USB connection first, since only one program can hold the port.
- **Battery % looks wrong:** trim Settings > Device > Battery divider until the voltage matches a multimeter.
