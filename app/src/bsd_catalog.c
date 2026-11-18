/*
 * Starter catalog for the demo. Each variant he recognizes is its own entry.
 * Names only (no pictures). Tiers are a game-balance setting; tune freely.
 */
#include "bsd_cars.h"

#include <string.h>

static const bsd_car_t s_catalog[] = {
    /* common — frequent, achievable finds */
    { "honda_crv",            "Honda",      "CR-V",               BSD_COMMON },
    { "toyota_rav4",          "Toyota",     "RAV4",               BSD_COMMON },
    { "ford_f150",            "Ford",       "F-150",              BSD_COMMON },
    { "honda_civic",          "Honda",      "Civic",              BSD_COMMON },
    { "toyota_corolla",       "Toyota",     "Corolla",            BSD_COMMON },
    { "toyota_camry",         "Toyota",     "Camry",              BSD_COMMON },
    { "jeep_wrangler",        "Jeep",       "Wrangler",           BSD_COMMON },
    { "ford_bronco_sport",    "Ford",       "Bronco Sport",       BSD_COMMON },
    { "tesla_model_y",        "Tesla",      "Model Y",            BSD_COMMON },
    { "subaru_outback",       "Subaru",     "Outback",            BSD_COMMON },

    /* uncommon */
    { "ford_bronco",          "Ford",       "Bronco",             BSD_UNCOMMON },
    { "ford_mustang",         "Ford",       "Mustang",            BSD_UNCOMMON },
    { "ford_maverick",        "Ford",       "Maverick",           BSD_UNCOMMON },
    { "jeep_gladiator",       "Jeep",       "Gladiator",          BSD_UNCOMMON },
    { "chevrolet_corvette",   "Chevrolet",  "Corvette Stingray",  BSD_UNCOMMON },

    /* rare — occasional */
    { "ford_f150_raptor",     "Ford",       "F-150 Raptor",       BSD_RARE },
    { "honda_civic_type_r",   "Honda",      "Civic Type R",       BSD_RARE },
    { "toyota_gr_corolla",    "Toyota",     "GR Corolla",         BSD_RARE },
    { "porsche_911_gt3",      "Porsche",    "911 GT3",            BSD_RARE },
    { "mercedes_amg_g63",     "Mercedes-AMG","G 63 4MATIC Grand Edition", BSD_RARE }, /* long-name test */
};

int bsd_catalog_count(void) { return (int)(sizeof(s_catalog) / sizeof(s_catalog[0])); }

const bsd_car_t *bsd_catalog_get(int i)
{
    return (i >= 0 && i < bsd_catalog_count()) ? &s_catalog[i] : NULL;
}

int bsd_catalog_find(const char *id)
{
    for (int i = 0; i < bsd_catalog_count(); i++)
        if (strcmp(s_catalog[i].id, id) == 0) return i;
    return -1;
}
