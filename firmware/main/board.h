/*
 * board.h — Waveshare ESP32-S3-Touch-LCD-4.3 bring-up for LVGL 9.5.
 * Display (RGB565 parallel, 800x480), GT911 touch, CH422G IO expander.
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>

#define BOARD_H_RES 800
#define BOARD_V_RES 480

/* Brings up I2C, expander, panel and touch, then registers an LVGL display
 * and pointer input. Call once, after lv_init(), from the LVGL task. */
esp_err_t board_init(void);

/* Monotonic milliseconds (esp_timer). Also used as LVGL's tick. */
uint32_t board_millis(void);

/* Backlight on/off through CH422G EXIO2 (the board has no dimming wired here). */
esp_err_t board_backlight(int on);
