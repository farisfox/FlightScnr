#pragma once

/**
 * Waveshare ESP32-S3-Touch-AMOLED-1.75C — 466×466 round AMOLED, QSPI
 * (CO5300 panel + CST9217 touch). ESP32-S3R8, 8 MB octal PSRAM, 32 MB flash.
 * https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75
 *
 * The "C" variant is wired differently from the plain 1.75 (LCD reset, touch
 * reset, I2S MCLK and the PWR button all move; no microSD, RTC or TCA9554).
 * Do not copy pins from non-C examples. See docs/boards/waveshare-amoled-175c.md.
 *
 * Inputs: there is no rotary encoder. The touchscreen's outer ring works as a
 * virtual knob (drag along the edge), and the BOOT button (GPIO 0) acts as the
 * knob push button (short press = knob tap, hold 5 s = Wi-Fi reset).
 * No piezo buzzer or haptic motor (the ES8311 speaker codec is not used yet).
 */

#define FLIGHTSCNR_BOARD_NAME "waveshare-amoled-175c"
#define FLIGHTSCNR_BOARD_WAVESHARE_AMOLED_175C 1

#define FLIGHTSCNR_HAS_KNOB_BUTTON 1
#define FLIGHTSCNR_HAS_ENCODER 0
#define FLIGHTSCNR_HAS_TOUCH_DIAL 1
#define FLIGHTSCNR_HAS_CST9217 1
#define FLIGHTSCNR_HAS_BUZZER 0
#define FLIGHTSCNR_HAS_HAPTIC 0
#define FLIGHTSCNR_HAS_LCD_VCI_EN 0
#define FLIGHTSCNR_HAS_LCD_BACKLIGHT_PWM 0
#define FLIGHTSCNR_PANEL_AUTODETECT 0

#ifndef CST9217_SLAVE_ADDRESS
#define CST9217_SLAVE_ADDRESS 0x5A
#endif

/* Not fitted; defined so shared code that names the CST816 still compiles. */
#ifndef CST816_SLAVE_ADDRESS
#define CST816_SLAVE_ADDRESS 0x15
#endif

#define IIC_SDA 15
#define IIC_SCL 14

#define TOUCH_INT 11
#define TOUCH_RST 2

/* Touch → display coordinate fix-ups (0/1). Adjust if taps land mirrored. */
#define FLIGHTSCNR_TOUCH_SWAP_XY 0
#define FLIGHTSCNR_TOUCH_MIRROR_X 0
#define FLIGHTSCNR_TOUCH_MIRROR_Y 0

#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RST 1
#define LCD_TE 13
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
/** CO5300 column offset on this panel (from Waveshare's own example). */
#define LCD_COL_OFFSET 6

/** Arduino_GFX / CO5300 rotation (0–3). */
#define FLIGHTSCNR_DISPLAY_ROTATION 0

/** BOOT button, active low. Holding it while plugging in USB enters download mode. */
#define KNOB_KEY 0
#define FLIGHTSCNR_RESET_BUTTON_LABEL "BOOT"
/** PWR button sense (polarity unverified; not used by the firmware yet). */
#define PWR_KEY 3

/* No encoder: config.h still names these pins, so park them on unused values. */
#define KNOB_DATA_A -1
#define KNOB_DATA_B -1
