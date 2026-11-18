/*
 * bsd_ui.h — the single child-facing screen (LVGL).
 * Shared verbatim by the desktop simulator and the ESP32 firmware.
 */
#ifndef BSD_UI_H
#define BSD_UI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Monotonic milliseconds; supplied by the platform (SDL ticks / esp_timer). */
typedef uint32_t (*bsd_clock_fn)(void);

void bsd_ui_create(bsd_clock_fn clock);

/* All bsd_ui_* calls must run on the LVGL task.
 * Call every ~100 ms: redraws only what changed, and re-evaluates staleness. */
void bsd_ui_refresh(void);

/* Fade-in of text that changed (default on). Off = text swaps instantly. */
void bsd_ui_set_animations(bool on);

/* True while the "This Drive" garage screen is showing. */
bool bsd_ui_garage_visible(void);

/* ---- test hooks (simulator) ---- */
/* Show this make/name instead of the current challenge (NULL to clear). */
void bsd_ui_debug_show_car(const char *make, const char *name);
/* Prints any text whose ink leaves its cell; returns the number of spills.
 * Only built when BSD_UI_TEST_HOOKS is defined (the simulator defines it). */
int  bsd_ui_debug_check_layout(void);

#ifdef __cplusplus
}
#endif
#endif
