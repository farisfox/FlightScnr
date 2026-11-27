# 0004 — Port to the Waveshare ESP32-S3-Touch-AMOLED-1.75C

Status: accepted
Date: 2026-11-27
Context: adding a third board, the Waveshare ESP32-S3-Touch-AMOLED-1.75C
(466×466 CO5300 AMOLED, CST9217 touch, no rotary encoder), in the fork
`farisfox/FlightScnr`. Board details: [docs/boards/waveshare-amoled-175c.md](../boards/waveshare-amoled-175c.md).

## Decisions

### 1. Third compile-time board, not a separate firmware

The repo already selects boards at compile time (`include/hardware/boards/*.h`,
one PlatformIO env per board, a board marker for the WebFlasher). The 1.75C is
added the same way as `waveshare-amoled-175c`. New capability flags in the board
headers replace board-name checks where the new board needed them:

| Flag | Meaning |
|---|---|
| `FLIGHTSCNR_HAS_ENCODER` | Rotary encoder fitted (default 1) |
| `FLIGHTSCNR_HAS_TOUCH_DIAL` | Edge-of-screen drag acts as the encoder |
| `FLIGHTSCNR_HAS_CST9217` | CST9217 touch controller |
| `FLIGHTSCNR_HAS_CHSC5816` | CHSC5816 touch controller (T-Encoder only) |
| `FLIGHTSCNR_TAP_ACTS_AS_KNOB` | Derived: no knob button or no encoder, so screen taps take knob-tap duties |
| `FLIGHTSCNR_RESET_BUTTON_LABEL` | Name of the Wi-Fi reset button in UI text ("knob" / "BOOT") |

Defaults keep the T-Encoder and Knob 1.8 builds behaving as before.

### 2. Keep the existing toolchain

The board's community projects use the pioarduino platform (Arduino-ESP32 3.x)
and Arduino_GFX 1.6.5+. FlightScnr stays on `espressif32@6.5.0` and its vendored
Arduino_GFX 1.3.7, which already contains `Arduino_CO5300`. Moving all three
boards to a new core is a separate, riskier change. Consequence: flash is used
as 16 MB (ESP-IDF 4.4 does not address above 16 MB), which also keeps app images
and partitions identical across boards.

### 3. Own CST9217 reader instead of a library upgrade

No vendored library supports the CST9217. Newer SensorLib has
`TouchDrvCST92xx`, but upgrading the vendored SensorLib 0.1.4 would also touch
the T-Encoder's CHSC5816 path. A ~70-line polled reader following the same
register protocol (MIT, attributed in code) keeps the change contained.

### 4. Touch dial replaces the encoder

Every screen expects encoder steps (radar range, aircraft cycling, settings
values, radar facing, clock settings). Instead of designing new controls per
screen, an edge drag on the round screen generates the same encoder steps, so
the screen logic is unchanged. A touch becomes a dial turn only if it starts in
the outer 70 px ring and moves mostly tangentially; everything else stays a
tap or swipe, so existing navigation is unaffected. The BOOT button takes the
knob-button role (short press, 5 s Wi-Fi reset). On touch-first boards, a tap
also advances flight detail and settings focus (as on the Knob 1.8), and swipe
left on flight detail steps to the next aircraft.

### 5. OTA and WebFlasher follow this fork

Upstream releases have no 1.75C image, so OTA, web UI links, the release notes
and the WebFlasher point at `farisfox/FlightScnr`. The repo is one define,
`FLIGHTSCNR_GITHUB_REPO` in `include/config.h`, so pointing back upstream is a
one-line change if the port is merged there.

## Not done

- ES8311 speaker beeps (no buzzer on this board).
- AXP2101 PMU configuration and battery readout.
- QMI8658 IMU (e.g. auto-rotation).
- Use of the tearing-effect line and the PWR button.
- Larger fonts for the 466 px panel.
