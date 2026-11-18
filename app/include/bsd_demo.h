/*
 * bsd_demo.h — indoor demo replay. Writes sample roads/songs into the feed
 * with source = BSD_SRC_DEMO so the UI always labels it as demo data.
 */
#ifndef BSD_DEMO_H
#define BSD_DEMO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void bsd_demo_start(uint32_t now_ms);
void bsd_demo_stop(void);
bool bsd_demo_running(void);
void bsd_demo_tick(uint32_t now_ms);     /* call often; advances on its own */
void bsd_demo_next(uint32_t now_ms);     /* jump to next step now           */
void bsd_demo_set_paused(bool paused);   /* hold the current step           */
bool bsd_demo_paused(void);
const char *bsd_demo_step_label(void);   /* for logs / simulator title      */

#ifdef __cplusplus
}
#endif
#endif
