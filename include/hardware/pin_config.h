#pragma once

/**
 * Compile-time board pin map.
 *
 * Selected by PlatformIO build flag:
 *   -D FLIGHTSCNR_BOARD_TENCODER_PRO   (LilyGO T-Encoder Pro — default)
 *   -D FLIGHTSCNR_BOARD_WAVESHARE_KNOB_18  (Waveshare ESP32-S3-Knob-Touch-LCD-1.8)
 *   -D FLIGHTSCNR_BOARD_WAVESHARE_AMOLED_175C  (Waveshare ESP32-S3-Touch-AMOLED-1.75C)
 *
 * Run `pio run` (env: auto) to pick a connected board, or `pio run -e <board>`.
 */

#if defined(FLIGHTSCNR_BOARD_WAVESHARE_AMOLED_175C)
#include "boards/waveshare_amoled_175c.h"
#elif defined(FLIGHTSCNR_BOARD_WAVESHARE_KNOB_18)
#include "boards/waveshare_knob_18.h"
#elif defined(FLIGHTSCNR_BOARD_TENCODER_PRO)
#include "boards/tencoder_pro.h"
#else
/* Native/host unit tests and accidental bare builds default to T-Encoder Pro. */
#include "boards/tencoder_pro.h"
#endif

/* Capability defaults for boards that predate these flags. */
/** Name of the physical button used for knob press / Wi-Fi reset, for UI text. */
#ifndef FLIGHTSCNR_RESET_BUTTON_LABEL
#define FLIGHTSCNR_RESET_BUTTON_LABEL "knob"
#endif
#ifndef FLIGHTSCNR_HAS_CHSC5816
#define FLIGHTSCNR_HAS_CHSC5816 0
#endif
#ifndef FLIGHTSCNR_HAS_ENCODER
#define FLIGHTSCNR_HAS_ENCODER 1
#endif
#ifndef FLIGHTSCNR_HAS_CST9217
#define FLIGHTSCNR_HAS_CST9217 0
#endif
#ifndef FLIGHTSCNR_HAS_TOUCH_DIAL
#define FLIGHTSCNR_HAS_TOUCH_DIAL 0
#endif
#ifndef FLIGHTSCNR_TOUCH_SWAP_XY
#define FLIGHTSCNR_TOUCH_SWAP_XY 0
#endif
#ifndef FLIGHTSCNR_TOUCH_MIRROR_X
#define FLIGHTSCNR_TOUCH_MIRROR_X 0
#endif
#ifndef FLIGHTSCNR_TOUCH_MIRROR_Y
#define FLIGHTSCNR_TOUCH_MIRROR_Y 0
#endif
#ifndef LCD_COL_OFFSET
#define LCD_COL_OFFSET 0
#endif

/**
 * Touch-first board: no physical knob button, or no encoder. Screen taps then
 * take over knob-tap duties (advance aircraft, cycle settings focus).
 */
#if !FLIGHTSCNR_HAS_KNOB_BUTTON || !FLIGHTSCNR_HAS_ENCODER
#define FLIGHTSCNR_TAP_ACTS_AS_KNOB 1
#else
#define FLIGHTSCNR_TAP_ACTS_AS_KNOB 0
#endif
