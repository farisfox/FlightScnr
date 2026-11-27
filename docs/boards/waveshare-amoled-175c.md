# Waveshare ESP32-S3-Touch-AMOLED-1.75C

FlightScnr port for the Waveshare **ESP32-S3-Touch-AMOLED-1.75C**: a 1.75" round
466×466 AMOLED with capacitive touch and no rotary knob. Build env:
`waveshare-amoled-175c`. Board header:
[`include/hardware/boards/waveshare_amoled_175c.h`](../../include/hardware/boards/waveshare_amoled_175c.h).
Design decisions are in [ADR 0004](../adr/0004-waveshare-amoled-175c-port.md).

> **Status:** compiles in CI. First hardware bring-up is still to be confirmed
> (see [Still to verify on hardware](#still-to-verify-on-hardware)).

## Board summary

| | |
|---|---|
| MCU / memory | ESP32-S3R8, 8 MB octal PSRAM, 32 MB flash (built as 16 MB, see below) |
| Display | CO5300 round AMOLED, 466×466, QSPI, no backlight pin (brightness = panel command 0x51) |
| Touch | CST9217 on I2C `0x5A` |
| Other I2C | AXP2101 PMIC `0x34`, QMI8658 IMU `0x6B`, ES8311 speaker codec `0x18`, ES7210 mic ADC `0x40` (only the ES8311 is used, for beeps) |
| Buttons | BOOT (GPIO 0, active low), PWR (GPIO 3 sense) |
| USB | ESP32-S3 native USB only (no CH343 bridge) |
| Not fitted | microSD, RTC, TCA9554 I/O expander, buzzer, vibration motor, rotary encoder |

## Pin map used by the firmware

| Function | GPIO |
|---|---|
| LCD QSPI D0–D3 | 4, 5, 6, 7 |
| LCD clock / CS | 38 / 12 |
| LCD reset | 1 |
| LCD tearing effect | 13 (defined, unused) |
| I2C SDA / SCL | 15 / 14 |
| Touch interrupt / reset | 11 / 2 |
| BOOT button (knob-press role) | 0 |
| PWR button sense | 3 (defined, unused) |

### The C is not the plain 1.75

Waveshare's non-C docs and examples look almost identical but are wired
differently. Do not copy pins from them.

| | 1.75C | 1.75 (non-C) |
|---|---|---|
| LCD reset | GPIO 1 | GPIO 39 |
| Touch reset | GPIO 2 | GPIO 40 |
| I2S MCLK | GPIO 16 | GPIO 42 |
| PWR button | GPIO 3 | TCA9554 expander P4 |
| microSD / RTC | none | yes |
| Flash | 32 MB | 16 MB |

## Controls

The 1.75C has no rotary encoder, so the touchscreen stands in for it.

| Action | 1.75C |
|---|---|
| Turn the knob (range, aircraft, settings values, facing, clock settings) | **Drag a finger around the outer edge of the screen.** Clockwise = knob clockwise. About every 18° of travel is one step. |
| Knob short press | Short press **BOOT**, or tap the screen where the other boards accept a tap |
| Next aircraft on flight detail | Tap the screen, swipe left, or edge-drag |
| Cycle the focused row on settings pages 2–3 and clock settings | Tap the screen |
| Back | Swipe right (unchanged) |
| Clear Wi-Fi and open the setup portal | Hold **BOOT** for 5 s |

How the edge dial decides: a touch that starts within 70 px of the rim and
moves mostly *along* the rim becomes a dial turn. A touch that starts nearer the
center, or moves mostly toward or away from the center, stays a normal tap or
swipe. Swipes therefore still work from anywhere, including the edge, as long as
they head across the screen rather than around it. The thresholds live at the
top of the touch-dial block in `src/hardware/input.cpp`.

Holding **BOOT** while plugging in USB enters the ESP32-S3 download mode, as on
any ESP32-S3 board.

## Build and flash

```bash
pio run -e waveshare-amoled-175c -t upload
# or for the auto env:
FLIGHTSCNR_BOARD=waveshare-amoled-175c pio run -t upload
```

The `auto` env cannot detect this board from USB (it only has native USB, like
the T-Encoder), so pick it by env name or `FLIGHTSCNR_BOARD` (aliases:
`amoled-175c`, `175c`).

If the serial port does not appear, hold **BOOT**, plug in USB, then release.

**WebFlasher:** select **Waveshare Touch AMOLED 1.75C** manually on the first
install. Later installs are auto-detected from the FlightScnr board marker
(`FSBRDMK:waveshare-amoled-175c`) already in flash.

**OTA:** the web portal's **Firmware update** card downloads
`FlightScnr-waveshare-amoled-175c-app.bin` from the latest release of this fork
(`farisfox/FlightScnr`, set by `FLIGHTSCNR_GITHUB_REPO` in `include/config.h`).

### Why 16 MB on a 32 MB board

The project builds with `espressif32@6.5.0` (Arduino-ESP32 2.x, ESP-IDF 4.4),
which does not address flash above 16 MB. The env therefore keeps the shared
`esp32s3_flash_16MB` board and `default_16MB.csv` partitions. The upper 16 MB is
unused, and app images have the same layout as the other boards.

## Software notes

- **Display:** the bundled Arduino_GFX 1.3.7 `Arduino_CO5300` driver (already used
  for the T-Encoder's TFD12 panel), at 40 MHz QSPI, with a **column offset of 6**
  (`LCD_COL_OFFSET`, taken from Waveshare's example). Pixel writes are 2-aligned
  as for the other CO5300 panel. Brightness uses the existing CO5300 path
  (command 0x51); there is no backlight PWM.
- **Touch:** a small CST9217 reader in `src/hardware/input.cpp`. It follows the
  register protocol of [lewisxhe/SensorLib](https://github.com/lewisxhe/SensorLib)
  `TouchDrvCST92xx` (MIT): read 16-bit register `0xD000` (15 bytes), check the
  `0xAB` marker at byte 6, then write `0xAB` back to acknowledge. It polls every
  10 ms. A touch with no fresh frame for 150 ms counts as released.
- **Touch orientation:** if taps land mirrored or rotated, set
  `FLIGHTSCNR_TOUCH_SWAP_XY`, `FLIGHTSCNR_TOUCH_MIRROR_X` or
  `FLIGHTSCNR_TOUCH_MIRROR_Y` in the board header.
- **Layout:** the radar, flight detail, clock, settings and disclaimer screens
  already lay out from the panel size. The forecast screen has its own 466 px
  geometry (the 390 px layout scaled by about 1.2), and the Wi-Fi sonar
  animation scales with the panel. Fonts are the same as on the 390 px
  T-Encoder, so text is relatively smaller on the bigger screen.
- **PMU:** the AXP2101 is left at its power-on defaults; other projects bring the
  panel up without configuring it. If the screen stays black, check the PMU
  rails first.
- **Sound:** no buzzer, so UI beeps and aircraft alerts are short sine tones
  played through the ES8311 codec and speaker. The codec is configured over I2C
  (`0x18`) at boot for 16 kHz / 16-bit with MCLK = 256 x fs, then tones go out
  over I2S (MCLK 16, BCLK 9, WS 45, DOUT 8). The "Beep tone" setting A to E
  scales pitch and loudness. If the serial log says `Speaker: ES8311 init
  failed`, the codec did not answer on the I2C bus. If it says ready but you
  hear nothing, the amplifier probably needs enabling: set `SPEAKER_PA_EN` in
  the board header to the right GPIO. The I2S pins other than MCLK are carried
  over from the non-C board and are unconfirmed.

## Still to verify on hardware

0. Speaker: beeps on tap and the three-beep aircraft alert are audible, and the
   serial log shows `Speaker: ES8311 ready`.
1. Panel shows a full, centered image (column offset 6, rotation 0).
2. Touch coordinates match the display (orientation flags above).
3. CST9217 frames are read reliably by polling (no INT handling yet).
4. Edge-dial feel: step size (18°), ring width (70 px), tangential ratio (1.5).
5. BOOT short press and the 5 s Wi-Fi reset.
6. Brightness levels look right on this panel.
7. Forecast layout fits inside the circle.

Hardware notes this port is based on (sources: Waveshare's 1.75 repo and pinout
pages, arduino-esp32 PR #13145 which was tested on a physical 1.75C, and
valleytechsolutions/Dragon-Detector) were collected separately and are
summarized above.
