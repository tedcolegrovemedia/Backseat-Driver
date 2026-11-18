/*
 * LVGL config for the desktop simulator. Anything not set here takes LVGL's
 * default (lv_conf_internal.h). Keep display-relevant options (color depth,
 * fonts) in sync with firmware/sdkconfig.defaults so the sim looks like the board.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16            /* board is RGB565 */

#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_USER
#define LV_LOG_PRINTF 1

/* desktop window */
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL.h>
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_DIRECT
#define LV_SDL_BUF_COUNT 1
#define LV_SDL_FULLSCREEN 0
#define LV_SDL_DIRECT_EXIT 1

#define LV_USE_SNAPSHOT 1            /* --shots mode writes PNG-able frames */

/* fonts */
/* Montserrat 16 is used for the status line (demo badge, Bluetooth icon).
 * All other text uses the fonts in app/fonts. */
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0

#endif
