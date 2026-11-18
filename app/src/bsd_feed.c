#include "bsd_feed.h"

#include <stdio.h>
#include <string.h>

static bsd_feed_t s_feed;

static void copy_str(char *dst, size_t cap, const char *src)
{
    snprintf(dst, cap, "%s", src ? src : "");
}

void bsd_feed_init(void)
{
    memset(&s_feed, 0, sizeof(s_feed));
    s_feed.revision = 1;
}

const bsd_feed_t *bsd_feed_get(void) { return &s_feed; }

void bsd_feed_set_source(bsd_source_t src)
{
    if (s_feed.source != src) { s_feed.source = src; s_feed.revision++; }
}

void bsd_feed_set_link(bsd_link_t link)
{
    if (s_feed.link != link) { s_feed.link = link; s_feed.revision++; }
}

void bsd_feed_set_light(bsd_light_t light)
{
    if (s_feed.light != light) { s_feed.light = light; s_feed.revision++; }
}

void bsd_feed_set_road(const char *name, bsd_dir_kind_t kind, const char *direction, uint32_t now_ms)
{
    bsd_road_t *r = &s_feed.road;
    r->valid = (name && name[0]);
    copy_str(r->name, sizeof(r->name), name);
    r->dir_kind = (direction && direction[0]) ? kind : BSD_DIR_NONE;
    copy_str(r->direction, sizeof(r->direction), r->dir_kind == BSD_DIR_NONE ? "" : direction);
    r->updated_ms = now_ms;
    s_feed.revision++;
}

void bsd_feed_clear_road(void)
{
    memset(&s_feed.road, 0, sizeof(s_feed.road));
    s_feed.revision++;
}

void bsd_feed_set_song(const char *title, const char *artist, bool playing, uint32_t now_ms)
{
    bsd_song_t *s = &s_feed.song;
    s->valid = (title && title[0]);
    copy_str(s->title, sizeof(s->title), title);
    copy_str(s->artist, sizeof(s->artist), artist);
    s->playing = playing;
    s->updated_ms = now_ms;
    s_feed.revision++;
}

void bsd_feed_clear_song(void)
{
    memset(&s_feed.song, 0, sizeof(s_feed.song));
    s_feed.revision++;
}

static bsd_fresh_t freshness(bool valid, uint32_t updated, uint32_t now, uint32_t stale, uint32_t lost)
{
    if (!valid) return BSD_FRESH_LOST;
    uint32_t age = now - updated;           /* wraps safely */
    if (age >= lost) return BSD_FRESH_LOST;
    if (age >= stale) return BSD_FRESH_STALE;
    return BSD_FRESH_OK;
}

bsd_fresh_t bsd_feed_road_freshness(uint32_t now_ms)
{
    return freshness(s_feed.road.valid, s_feed.road.updated_ms, now_ms,
                     BSD_ROAD_STALE_MS, BSD_ROAD_LOST_MS);
}

bsd_fresh_t bsd_feed_song_freshness(uint32_t now_ms)
{
    /* Songs don't get a "dim" stage: either we trust it or we hide it. */
    return freshness(s_feed.song.valid, s_feed.song.updated_ms, now_ms,
                     BSD_SONG_LOST_MS, BSD_SONG_LOST_MS);
}
