/*
 * Challenge picker + drive garage.
 *
 * Picking rules:
 *   1. Choose a tier by weight (common/uncommon/rare).
 *   2. Within the tier, prefer cars not offered in the last N picks and not
 *      already found this drive.
 *   3. Never offer two rare cars in a row, so finds stay frequent.
 *   4. Fall back gracefully when the catalog is small.
 */
#include "bsd_cars.h"

#include <stdio.h>
#include <string.h>

#define RECENT_MAX 16

static bsd_game_cfg_t s_cfg;
static uint32_t       s_rng;
static int            s_current = -1;
static int            s_recent[RECENT_MAX];
static int            s_recent_len;
static bool           s_last_was_rare;
static uint32_t       s_rev = 1;

static bsd_sighting_t s_sightings[BSD_DRIVE_MAX_SIGHTINGS];
static int            s_sighting_count;
static uint32_t       s_drive_start_ms;

void bsd_game_default_cfg(bsd_game_cfg_t *cfg)
{
    cfg->weight[BSD_COMMON]   = 70;
    cfg->weight[BSD_UNCOMMON] = 25;
    cfg->weight[BSD_RARE]     = 5;
    cfg->no_repeat_window     = 6;
}

static uint32_t rnd(void)
{
    /* xorshift32 — deterministic, no libc dependency, fine for a game */
    uint32_t x = s_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s_rng = x;
}

static bool recently_offered(int idx)
{
    int window = s_cfg.no_repeat_window < s_recent_len ? s_cfg.no_repeat_window : s_recent_len;
    for (int i = 0; i < window; i++)
        if (s_recent[i] == idx) return true;
    return false;
}

static bool found_this_drive(int idx)
{
    for (int i = 0; i < s_sighting_count; i++)
        if (s_sightings[i].car_index == idx) return true;
    return false;
}

static void remember(int idx)
{
    memmove(&s_recent[1], &s_recent[0], sizeof(int) * (RECENT_MAX - 1));
    s_recent[0] = idx;
    if (s_recent_len < RECENT_MAX) s_recent_len++;
}

/* Pick a car from `tier` honoring the strictness level; -1 if none qualifies. */
static int pick_in_tier(bsd_rarity_t tier, int strict)
{
    /* Reservoir sampling: uniform choice with no size limit on the catalog. */
    int chosen = -1;
    uint32_t n = 0;
    for (int i = 0; i < bsd_catalog_count(); i++) {
        const bsd_car_t *c = bsd_catalog_get(i);
        if (c->rarity != tier || i == s_current) continue;
        if (strict >= 1 && recently_offered(i)) continue;
        if (strict >= 2 && found_this_drive(i)) continue;
        n++;
        if (rnd() % n == 0) chosen = i;
    }
    return chosen;
}

static bsd_rarity_t pick_tier(void)
{
    uint32_t w[BSD_RARITY_COUNT], total = 0;
    for (int t = 0; t < BSD_RARITY_COUNT; t++) {
        w[t] = s_cfg.weight[t];
        if (t == BSD_RARE && s_last_was_rare) w[t] = 0;
        total += w[t];
    }
    if (total == 0) return BSD_COMMON;
    uint32_t r = rnd() % total;
    for (int t = 0; t < BSD_RARITY_COUNT; t++) {
        if (r < w[t]) return (bsd_rarity_t)t;
        r -= w[t];
    }
    return BSD_COMMON;
}

static void advance(void)
{
    bsd_rarity_t tier = pick_tier();
    int next = -1;
    /* Relax rules step by step, then widen to other tiers. */
    for (int strict = 2; strict >= 0 && next < 0; strict--) next = pick_in_tier(tier, strict);
    for (int t = 0; t < BSD_RARITY_COUNT && next < 0; t++)
        for (int strict = 2; strict >= 0 && next < 0; strict--) next = pick_in_tier((bsd_rarity_t)t, strict);
    if (next < 0) next = s_current; /* catalog of one */

    s_current = next;
    s_last_was_rare = next >= 0 && bsd_catalog_get(next)->rarity == BSD_RARE;
    remember(next);
    s_rev++;
}

void bsd_game_init(const bsd_game_cfg_t *cfg, uint32_t seed, uint32_t now_ms)
{
    if (cfg) s_cfg = *cfg; else bsd_game_default_cfg(&s_cfg);
    s_rng = seed ? seed : 0x9E3779B9u;
    s_recent_len = 0;
    s_current = -1;
    s_last_was_rare = false;
    bsd_drive_start(now_ms);
}

void bsd_drive_start(uint32_t now_ms)
{
    s_sighting_count = 0;
    s_drive_start_ms = now_ms;
    advance();
}

int bsd_challenge_current(void) { return s_current; }

void bsd_challenge_found(uint32_t now_ms, const char *road)
{
    if (s_current >= 0 && s_sighting_count < BSD_DRIVE_MAX_SIGHTINGS) {
        bsd_sighting_t *s = &s_sightings[s_sighting_count++];
        s->car_index = (int16_t)s_current;
        s->at_ms = now_ms - s_drive_start_ms;
        snprintf(s->road, sizeof(s->road), "%s", road ? road : "");
    }
    advance();
}

void bsd_challenge_skip(void) { advance(); }

int bsd_drive_sighting_count(void) { return s_sighting_count; }

const bsd_sighting_t *bsd_drive_sighting(int i)
{
    return (i >= 0 && i < s_sighting_count) ? &s_sightings[i] : NULL;
}

uint32_t bsd_drive_started_ms(void) { return s_drive_start_ms; }
uint32_t bsd_game_revision(void) { return s_rev; }
