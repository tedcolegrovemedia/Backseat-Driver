/*
 * bsd_cars.h — car catalog, challenge picker and "This Drive" garage.
 *
 * Rarity is a game setting (how often a car is offered), not a claim about
 * how common the car is on Lehigh Valley roads.
 */
#ifndef BSD_CARS_H
#define BSD_CARS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { BSD_COMMON = 0, BSD_UNCOMMON, BSD_RARE, BSD_RARITY_COUNT } bsd_rarity_t;

typedef struct {
    const char  *id;        /* stable key                                      */
    const char  *make;      /* "Ford"                                         */
    const char  *name;      /* the name he knows it by: "F-150 Raptor"        */
    bsd_rarity_t rarity;
} bsd_car_t;

/* ---- catalog ---- */
int              bsd_catalog_count(void);
const bsd_car_t *bsd_catalog_get(int index);
int              bsd_catalog_find(const char *id);   /* -1 if missing */

/* ---- game settings ---- */
typedef struct {
    uint8_t weight[BSD_RARITY_COUNT];  /* relative odds per tier, e.g. 70/25/5 */
    uint8_t no_repeat_window;          /* don't re-offer any of the last N cars */
} bsd_game_cfg_t;

void bsd_game_default_cfg(bsd_game_cfg_t *cfg);

/* ---- drive garage ---- */
#define BSD_DRIVE_MAX_SIGHTINGS 256
#define BSD_SIGHTING_ROAD_MAX   64

typedef struct {
    int16_t  car_index;
    uint32_t at_ms;                         /* ms since the drive started */
    char     road[BSD_SIGHTING_ROAD_MAX];   /* road at the time, if known  */
} bsd_sighting_t;

/* ---- challenge flow ---- */
void bsd_game_init(const bsd_game_cfg_t *cfg, uint32_t seed, uint32_t now_ms);

/* Starts a new drive: clears the garage and picks a fresh car. */
void bsd_drive_start(uint32_t now_ms);

int  bsd_challenge_current(void);   /* catalog index of the car on screen */

/* "Found it": records the sighting for this drive, then advances. */
void bsd_challenge_found(uint32_t now_ms, const char *road_or_null);

/* "Skip": advances without recording. */
void bsd_challenge_skip(void);

int                   bsd_drive_sighting_count(void);
const bsd_sighting_t *bsd_drive_sighting(int i);
uint32_t              bsd_drive_started_ms(void);

/* Bumps whenever the current challenge or the garage changes. */
uint32_t bsd_game_revision(void);

#ifdef __cplusplus
}
#endif
#endif
