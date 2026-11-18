/*
 * bsd_feed.h — what the display knows about the outside world.
 *
 * The UI only reads this. Data sources (demo replay, BLE from the phone)
 * only write it. Nothing here knows about LVGL, BLE, Spotify or maps, so a
 * simulated source can be swapped for a live one without touching the UI.
 *
 * All times are milliseconds from a monotonic clock supplied by the caller.
 *
 * Not thread-safe. On the board the BLE callbacks run on another task, so
 * they must hand updates to the LVGL task (or take the LVGL lock) before
 * calling bsd_feed_set_*; the UI reads this struct without locking.
 */
#ifndef BSD_FEED_H
#define BSD_FEED_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSD_ROAD_NAME_MAX   96
#define BSD_DIR_MAX         16
#define BSD_SONG_TITLE_MAX  128
#define BSD_SONG_ARTIST_MAX 96

/* Where the current data came from. DEMO must never look like LIVE. */
typedef enum {
    BSD_SRC_NONE = 0,
    BSD_SRC_DEMO,
    BSD_SRC_LIVE,
} bsd_source_t;

/*
 * Signed direction ("Route 22 West") and travel bearing ("heading northwest")
 * are different facts and are shown differently. A source must only send
 * SIGNED when it has a reliable signed-direction value for the route.
 */
typedef enum {
    BSD_DIR_NONE = 0,  /* direction unknown: show the road name only     */
    BSD_DIR_SIGNED,    /* posted route direction: North/South/East/West  */
    BSD_DIR_BEARING,   /* instantaneous course of travel (compass point) */
} bsd_dir_kind_t;

typedef struct {
    bool           valid;
    char           name[BSD_ROAD_NAME_MAX];   /* "Route 22", "Hamilton Blvd" */
    bsd_dir_kind_t dir_kind;
    char           direction[BSD_DIR_MAX];    /* "West", "Northwest"         */
    uint32_t       updated_ms;
} bsd_road_t;

typedef struct {
    bool     valid;
    bool     playing;
    char     title[BSD_SONG_TITLE_MAX];
    char     artist[BSD_SONG_ARTIST_MAX];
    uint32_t updated_ms;
} bsd_song_t;

typedef enum {
    BSD_LINK_DISCONNECTED = 0,
    BSD_LINK_CONNECTING,
    BSD_LINK_CONNECTED,
} bsd_link_t;

/* Day/night look. The board has no clock or light sensor, so the phone
 * decides (it knows local time and sunset). Unknown is drawn as night,
 * which is the safer choice for glare. */
typedef enum {
    BSD_LIGHT_UNKNOWN = 0,
    BSD_LIGHT_DAY,
    BSD_LIGHT_NIGHT,
} bsd_light_t;

typedef struct {
    bsd_source_t source;
    bsd_link_t   link;
    bsd_light_t  light;
    bsd_road_t   road;
    bsd_song_t   song;
    uint32_t     revision;   /* bumps on every change; UI redraws when it moves */
} bsd_feed_t;

/* Freshness thresholds (tunable). */
#define BSD_ROAD_STALE_MS  15000u   /* dim the road after this    */
#define BSD_ROAD_LOST_MS   90000u   /* stop showing it after this */
#define BSD_SONG_LOST_MS   90000u   /* hide the song after this   */

typedef enum { BSD_FRESH_OK, BSD_FRESH_STALE, BSD_FRESH_LOST } bsd_fresh_t;

void              bsd_feed_init(void);
const bsd_feed_t *bsd_feed_get(void);

void bsd_feed_set_source(bsd_source_t src);
void bsd_feed_set_link(bsd_link_t link);
void bsd_feed_set_light(bsd_light_t light);
void bsd_feed_set_road(const char *name, bsd_dir_kind_t kind, const char *direction, uint32_t now_ms);
void bsd_feed_clear_road(void);
void bsd_feed_set_song(const char *title, const char *artist, bool playing, uint32_t now_ms);
void bsd_feed_clear_song(void);

bsd_fresh_t bsd_feed_road_freshness(uint32_t now_ms);
bsd_fresh_t bsd_feed_song_freshness(uint32_t now_ms);

#ifdef __cplusplus
}
#endif
#endif
