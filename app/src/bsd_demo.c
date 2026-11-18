/*
 * Scripted indoor replay: a short loop around the Lehigh Valley that
 * exercises every display state (signed direction, bearing only, no
 * direction, long names, lost location, lost song, phone disconnect).
 * The first half of the loop is night, the second half day.
 */
#include "bsd_demo.h"
#include "bsd_feed.h"

#include <stddef.h>

typedef enum { ROAD_SET, ROAD_HOLD, ROAD_CLEAR } road_op_t;
typedef enum { SONG_SET, SONG_HOLD, SONG_CLEAR } song_op_t;

typedef struct {
    const char    *label;
    uint32_t       duration_ms;
    road_op_t      road_op;
    const char    *road;
    bsd_dir_kind_t dir_kind;
    const char    *dir;
    song_op_t      song_op;
    const char    *title;
    const char    *artist;
    bsd_link_t     link;
    bsd_light_t    light;
} step_t;

static const step_t s_script[] = {
    { "Route 22 West (signed)",        12000, ROAD_SET, "Route 22", BSD_DIR_SIGNED, "West",
      SONG_SET, "Here Comes the Sun", "The Beatles", BSD_LINK_CONNECTED, BSD_LIGHT_NIGHT },
    { "Route 378 North (signed)",      12000, ROAD_SET, "Route 378", BSD_DIR_SIGNED, "North",
      SONG_HOLD, NULL, NULL, BSD_LINK_CONNECTED, BSD_LIGHT_NIGHT },
    { "Local street, bearing only",    12000, ROAD_SET, "Hamilton Boulevard", BSD_DIR_BEARING, "Southwest",
      SONG_SET, "Life Is a Highway", "Rascal Flatts", BSD_LINK_CONNECTED, BSD_LIGHT_NIGHT },
    { "Long road + long song",         12000, ROAD_SET, "Northeast Extension of the Pennsylvania Turnpike", BSD_DIR_SIGNED, "North",
      SONG_SET, "The Longest Song Title We Could Think Of For Testing (Extended Live Version)",
      "The Lehigh Valley Philharmonic Orchestra and Friends", BSD_LINK_CONNECTED, BSD_LIGHT_NIGHT },
    { "Road with no direction",        10000, ROAD_SET, "Cedar Crest Boulevard", BSD_DIR_NONE, NULL,
      SONG_CLEAR, NULL, NULL, BSD_LINK_CONNECTED, BSD_LIGHT_DAY },
    { "Location goes quiet (stale)",   20000, ROAD_HOLD, NULL, BSD_DIR_NONE, NULL,
      SONG_SET, "Drive My Car", "The Beatles", BSD_LINK_CONNECTED, BSD_LIGHT_DAY },
    { "Phone disconnected",            10000, ROAD_HOLD, NULL, BSD_DIR_NONE, NULL,
      SONG_HOLD, NULL, NULL, BSD_LINK_DISCONNECTED, BSD_LIGHT_DAY },
    { "Reconnecting",                   4000, ROAD_CLEAR, NULL, BSD_DIR_NONE, NULL,
      SONG_CLEAR, NULL, NULL, BSD_LINK_CONNECTING, BSD_LIGHT_DAY },
    { "Interstate 78 East (signed)",   12000, ROAD_SET, "Interstate 78", BSD_DIR_SIGNED, "East",
      SONG_SET, "Bluebird", "Alexa Rose", BSD_LINK_CONNECTED, BSD_LIGHT_DAY },
};
#define STEP_COUNT ((int)(sizeof(s_script) / sizeof(s_script[0])))

static bool     s_running;
static bool     s_paused;
static int      s_step;
static uint32_t s_step_started;

static void apply(int i, uint32_t now)
{
    const step_t *st = &s_script[i];
    bsd_feed_set_link(st->link);
    bsd_feed_set_light(st->light);
    if (st->road_op == ROAD_SET)   bsd_feed_set_road(st->road, st->dir_kind, st->dir, now);
    if (st->road_op == ROAD_CLEAR) bsd_feed_clear_road();
    if (st->song_op == SONG_SET)   bsd_feed_set_song(st->title, st->artist, true, now);
    if (st->song_op == SONG_CLEAR) bsd_feed_clear_song();
    s_step = i;
    s_step_started = now;
}

/* A live phone resends the same road every few seconds; mimic that so data
 * stays fresh, except during steps that deliberately go quiet. */
static void refresh(uint32_t now)
{
    const step_t *st = &s_script[s_step];
    const bsd_feed_t *f = bsd_feed_get();
    if (st->road_op == ROAD_SET && f->road.valid && now - f->road.updated_ms > 3000)
        bsd_feed_set_road(f->road.name, f->road.dir_kind, f->road.direction, now);
    if (st->link == BSD_LINK_CONNECTED && st->road_op != ROAD_HOLD && f->song.valid &&
        now - f->song.updated_ms > 5000)
        bsd_feed_set_song(f->song.title, f->song.artist, f->song.playing, now);
}

void bsd_demo_start(uint32_t now)
{
    s_running = true;
    s_paused = false;
    bsd_feed_set_source(BSD_SRC_DEMO);
    apply(0, now);
}

void bsd_demo_stop(void)
{
    s_running = false;
    bsd_feed_set_source(BSD_SRC_NONE);
}

bool bsd_demo_running(void) { return s_running; }

void bsd_demo_tick(uint32_t now)
{
    if (!s_running) return;
    if (!s_paused && now - s_step_started >= s_script[s_step].duration_ms)
        apply((s_step + 1) % STEP_COUNT, now);
    else
        refresh(now);
}

void bsd_demo_next(uint32_t now)
{
    if (s_running) apply((s_step + 1) % STEP_COUNT, now);
}

void bsd_demo_set_paused(bool p) { s_paused = p; }
bool bsd_demo_paused(void) { return s_paused; }

const char *bsd_demo_step_label(void)
{
    return s_running ? s_script[s_step].label : "demo off";
}
