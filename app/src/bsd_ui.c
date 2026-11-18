/*
 * Child-facing screen, implemented from the "Backseat Driver" Figma frame
 * (node 1:2), 800x480.
 *
 *   y  16        "currently traveling on:"            [DEMO badge / phone status]
 *   y  57-193    | ROAD NAME (2 lines max)           | DIRECTION |
 *   y 193-331    | look out for a | Car Make          [FOUND IT!]
 *                |                | Car Model         [  SKIP   ]
 *   y 350        "Music Playing"                              [ GARAGE  ]
 *   y 391        ----------------------------------------------
 *   y 410        Artist   Song Name
 *
 * Type is placed optically: each text block's capital band (cap top to last
 * baseline) is centered on its row, and left edges are corrected for the
 * first glyph's side bearing so letters (not label boxes) line up.
 *
 * Two themes, chosen by bsd_feed light: night (dark asphalt grain, the
 * Figma colors) and day (light concrete grain, inverted). Textures are
 * generated at boot as 128x128 RGB565 tiles; no image files.
 *
 * A second screen, the "This Drive" garage, uses the same grid, fonts,
 * textures and theme: BACK in the header, then a two-column table of the
 * cars spotted this drive (newest first). It returns to the main screen on
 * its own after GARAGE_IDLE_MS without a touch.
 *
 * Brief rules: no sounds, no animations, no scores/timers, nothing that
 * needs continuous attention. Buttons have no press transitions.
 */
#include "bsd_ui.h"
#include "bsd_cars.h"
#include "bsd_feed.h"
#include "../fonts/bsd_fonts.h"

#include "lvgl.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ---- geometry (Figma px) ---- */
#define GRID_L      26
#define GRID_R      775
#define GRID_W      (GRID_R - GRID_L)
#define ROW_ROAD_T  57
#define ROW_CAR_T   193
#define ROW_CAR_B   331
#define MUSIC_RULE  391
#define SCR_H       480
#define DIR_X       636            /* divider between road name and direction */
#define LOOK_X      117            /* divider after "look out for a"          */

#define ROAD_TEXT_X GRID_L
#define ROAD_TEXT_W (DIR_X - GRID_L - 16)
#define CAR_TEXT_X  137
#define CAR_TEXT_W  (BTN_X - CAR_TEXT_X - 16)
#define DIR_W       (GRID_R - DIR_X - 14)

#define BTN_X       620
#define BTN_W       155
/* Taller than the Figma's 155x47 for small fingers (see docs/enclosure.md):
 * FOUND IT! 76 px (~8.5 mm on the glass), SKIP 48 px (~5.4 mm), 8 px gap. */
#define FOUND_Y     196
#define FOUND_H     76
#define SKIP_Y      280
#define SKIP_H      48

#define GBTN_Y      344            /* small GARAGE button, music-header row */
#define GBTN_H      34

/* garage screen */
#define G_ROW_H     84
#define G_COL_W     375
#define G_TEXT_PAD  20
#define GARAGE_IDLE_MS 45000

/* optical centers of each text row (center of the capital band) */
#define CY_HEADER   28
#define CY_ROAD     ((ROW_ROAD_T + ROW_CAR_T) / 2)
#define CY_CAR      ((ROW_CAR_T + ROW_CAR_B) / 2)
#define CY_MUSIC_HD ((ROW_CAR_B + MUSIC_RULE) / 2)
#define CY_MUSIC    ((MUSIC_RULE + SCR_H) / 2 - 2)

#define MAKE_MODEL_GAP 14          /* make baseline -> model cap top */
#define ARTIST_MAX_W   330
#define ARTIST_SONG_GAP 52

#define TAP_GUARD_MS 700   /* ignore double-taps right after the car changes */

/* Text change animation: new text fades in while settling up a few pixels.
 * Kept short and calm (no bounce, no repeat) per the brief's "no distracting
 * effects"; only text whose content actually changed animates. */
#define ANIM_MS      320
#define ANIM_RISE    8     /* px the text settles upward */
#define ANIM_STAGGER 70    /* ms between the two lines of a pair */

/* ---- themes ---- */
typedef struct {
    uint32_t   base;           /* texture mid-tone                       */
    int        grain;          /* +/- per-pixel noise                    */
    int        mottle;         /* +/- low-frequency variation            */
    int        speck;          /* speck brightness offset (+ or -)       */
    lv_color_t text, line, dim, stale;
    lv_color_t found_bg, found_tx, found_pr;
    lv_color_t skip_bd, skip_tx, skip_pr;
} theme_t;

static const theme_t *theme_night(void)
{
    static theme_t t;
    t.base = 0x0E0E0F; t.grain = 6; t.mottle = 3; t.speck = 16;
    t.text = lv_color_hex(0xFFFFFF);  t.line = lv_color_hex(0x2E2E2E);
    t.dim  = lv_color_hex(0x9A9A9A);  t.stale = lv_color_hex(0x5A5A5A);
    t.found_bg = lv_color_hex(0xD9D9D9); t.found_tx = lv_color_hex(0x000000); t.found_pr = lv_color_hex(0xA6A6A6);
    t.skip_bd  = lv_color_hex(0x707070); t.skip_tx  = lv_color_hex(0xFFFFFF); t.skip_pr  = lv_color_hex(0x2A2A2A);
    return &t;
}

static const theme_t *theme_day(void)
{
    static theme_t t;
    t.base = 0xE6E4DF; t.grain = 7; t.mottle = 5; t.speck = -22;
    t.text = lv_color_hex(0x111111);  t.line = lv_color_hex(0xBDBAB3);
    t.dim  = lv_color_hex(0x666666);  t.stale = lv_color_hex(0xA3A09A);
    t.found_bg = lv_color_hex(0x1E1E1E); t.found_tx = lv_color_hex(0xFFFFFF); t.found_pr = lv_color_hex(0x4A4A4A);
    t.skip_bd  = lv_color_hex(0x7A7A7A); t.skip_tx  = lv_color_hex(0x111111); t.skip_pr  = lv_color_hex(0xCFCCC5);
    return &t;
}

#define C_DEMO      lv_color_hex(0xE0A100)
#define C_DEMO_TX   lv_color_hex(0x000000)
#define C_WARN      lv_color_hex(0xD9822B)

/* ---- state ---- */
static bsd_clock_fn   s_clock;
static const theme_t *s_theme;

static lv_obj_t *s_rules[8];
static lv_obj_t *s_header, *s_demo_badge, *s_link_label;
static lv_obj_t *s_road_label, *s_dir_label, *s_dir_caption;
static lv_obj_t *s_look, *s_car_make, *s_car_model;
static lv_obj_t *s_btn_found, *s_btn_skip, *s_btn_found_l, *s_btn_skip_l;
static lv_obj_t *s_music_header, *s_artist, *s_song;
static lv_obj_t *s_btn_garage, *s_btn_garage_l;
static lv_obj_t *s_main_scr, *s_garage_scr, *s_garage_list;

static uint32_t s_seen_feed_rev, s_seen_game_rev;
static int      s_seen_road_fresh = -1, s_seen_song_fresh = -1;
static uint32_t s_last_advance_ms;
static const char *s_dbg_make, *s_dbg_name;   /* test override for the car text */
static bool        s_animate = true;

/* ------------------------------------------------------------------ textures */

#define TILE 128
/* Heap, not .bss: 2 x 32 KB would otherwise sit in the ESP32's scarce
 * internal RAM. With LVGL on the C allocator these land in PSRAM. If an
 * allocation fails the screen falls back to the flat base color. */
static uint16_t      *s_tile_px[2];
static lv_image_dsc_t s_tile[2];

static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

/* tileable value noise in [-1,1] on a CxC lattice */
static float vnoise(int x, int y, int cells, uint32_t seed)
{
    int cs = TILE / cells;
    int gx = x / cs, gy = y / cs;
    float fx = (float)(x % cs) / cs, fy = (float)(y % cs) / cs;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
#define LAT(a, b) ((float)(hash32(seed ^ (uint32_t)(((a) % cells) * 7919 + ((b) % cells) * 104729)) & 0xFFFF) / 32767.5f - 1.0f)
    float v00 = LAT(gx, gy), v10 = LAT(gx + 1, gy), v01 = LAT(gx, gy + 1), v11 = LAT(gx + 1, gy + 1);
#undef LAT
    float a = v00 + (v10 - v00) * fx, b = v01 + (v11 - v01) * fx;
    return a + (b - a) * fy;
}

static int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

static void make_tile(int idx, const theme_t *t, uint32_t seed)
{
    int br = (t->base >> 16) & 0xFF, bg = (t->base >> 8) & 0xFF, bb = t->base & 0xFF;
    if (!s_tile_px[idx]) s_tile_px[idx] = lv_malloc(TILE * TILE * sizeof(uint16_t));
    if (!s_tile_px[idx]) return;
    for (int y = 0; y < TILE; y++) {
        for (int x = 0; x < TILE; x++) {
            uint32_t h = hash32(seed + (uint32_t)(y * TILE + x) * 2654435761U);
            int d = (int)(h % (2 * t->grain + 1)) - t->grain;
            d += (int)(t->mottle * (vnoise(x, y, 8, seed) * 0.65f + vnoise(x, y, 16, seed * 3) * 0.35f));
            if ((h >> 20) % 97 == 0) d += t->speck;          /* sparse aggregate specks */
            lv_color_t c = lv_color_make(clamp255(br + d), clamp255(bg + d), clamp255(bb + d));
            s_tile_px[idx][y * TILE + x] = lv_color_to_u16(c);
        }
    }
    lv_image_dsc_t *dsc = &s_tile[idx];
    memset(dsc, 0, sizeof(*dsc));
    dsc->header.magic  = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf     = LV_COLOR_FORMAT_RGB565;
    dsc->header.w      = TILE;
    dsc->header.h      = TILE;
    dsc->header.stride = TILE * 2;
    dsc->data_size     = TILE * TILE * sizeof(uint16_t);
    dsc->data          = (const uint8_t *)s_tile_px[idx];
}

/* ------------------------------------------------------------------ type helpers */

static uint32_t now_ms(void) { return s_clock ? s_clock() : lv_tick_get(); }

static lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *font)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_label_set_text(l, "");
    return l;
}

static int32_t f_ascent(const lv_font_t *f) { return f->line_height - f->base_line; }

static int32_t f_cap(const lv_font_t *f)
{
    lv_font_glyph_dsc_t g;
    if (lv_font_get_glyph_dsc(f, &g, 'H', 0)) return g.box_h + g.ofs_y;
    return f_ascent(f) * 7 / 10;
}

/* Overpass has tall line metrics; pull wrapped lines together so two-line
 * names look like the Figma (line pitch ~1.2x the font size). */
static int32_t tight_line_space(const lv_font_t *f)
{
    return (f_ascent(f) * 6) / 5 - f->line_height;   /* negative */
}

static int label_lines(lv_obj_t *lbl)
{
    const lv_font_t *f = lv_obj_get_style_text_font(lbl, 0);
    int32_t ls = lv_obj_get_style_text_line_space(lbl, 0);
    lv_obj_update_layout(lbl);
    int32_t h = lv_obj_get_height(lbl);
    int n = (int)((h + ls) / (f->line_height + ls));
    return n < 1 ? 1 : n;
}

/* Distance from the label's top edge to its first cap top / last baseline. */
static int32_t ink_top_off(lv_obj_t *lbl)
{
    const lv_font_t *f = lv_obj_get_style_text_font(lbl, 0);
    return f_ascent(f) - f_cap(f);
}

static int32_t ink_base_off(lv_obj_t *lbl)
{
    const lv_font_t *f = lv_obj_get_style_text_font(lbl, 0);
    int32_t ls = lv_obj_get_style_text_line_space(lbl, 0);
    return (label_lines(lbl) - 1) * (f->line_height + ls) + f_ascent(f);
}

/* Left side bearing of the first glyph, so letters (not boxes) align. */
static int32_t first_bearing(lv_obj_t *lbl)
{
    const char *t = lv_label_get_text(lbl);
    if (!t || !t[0]) return 0;
    lv_font_glyph_dsc_t g;
    const lv_font_t *f = lv_obj_get_style_text_font(lbl, 0);
    if (lv_font_get_glyph_dsc(f, &g, (uint32_t)(unsigned char)t[0], (uint32_t)(unsigned char)t[1])) return g.ofs_x;
    return 0;
}

/* Place `lbl` so its capital band is centered on `cy`. If `optical_x`, the
 * first glyph's ink starts exactly at `x`; otherwise the box starts at `x`. */
static int32_t place_text(lv_obj_t *lbl, int32_t x, int32_t cy, bool optical_x)
{
    lv_obj_update_layout(lbl);
    int32_t top = ink_top_off(lbl), base = ink_base_off(lbl);
    int32_t y = cy - (top + base) / 2;
    lv_obj_set_pos(lbl, optical_x ? x - first_bearing(lbl) : x, y);
    return y;   /* returned because lv_obj_get_y() lags until the next layout pass */
}

/* Put `txt` in `lbl` using the largest font that fits `max_lines` lines in
 * `width`. If nothing fits, use the smallest font and truncate with "...".
 * No scrolling marquee: moving text is a distraction. */
static void fit_text(lv_obj_t *lbl, const char *txt, const lv_font_t *const *fonts, int nfonts,
                     int32_t width, int max_lines)
{
    for (int i = 0; i < nfonts; i++) {
        const lv_font_t *f = fonts[i];
        int32_t ls = tight_line_space(f);
        lv_point_t sz;
        lv_text_get_size(&sz, txt, f, 0, ls, width, LV_TEXT_FLAG_NONE);
        int32_t lines = (sz.y + ls) / (f->line_height + ls);
        if (lines <= max_lines && sz.x <= width) {
            lv_obj_set_style_text_font(lbl, f, 0);
            lv_obj_set_style_text_line_space(lbl, ls, 0);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
            lv_obj_set_size(lbl, width, LV_SIZE_CONTENT);
            lv_label_set_text(lbl, txt);
            return;
        }
    }
    const lv_font_t *f = fonts[nfonts - 1];
    int32_t ls = tight_line_space(f);
    lv_obj_set_style_text_font(lbl, f, 0);
    lv_obj_set_style_text_line_space(lbl, ls, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_size(lbl, width, f->line_height * max_lines + ls * (max_lines - 1));
    lv_label_set_text(lbl, txt);
}

static void to_upper(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++) dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

/* ------------------------------------------------------------------ change animation */

static void anim_in_cb(void *obj, int32_t v)          /* v: 0 .. 256 */
{
    lv_obj_set_style_opa(obj, (lv_opa_t)(v >= 256 ? 255 : v), 0);
    lv_obj_set_style_translate_y(obj, ((256 - v) * ANIM_RISE) / 256, 0);
}

static void animate_in(lv_obj_t *o, uint32_t delay)
{
    lv_anim_delete(o, anim_in_cb);
    if (!s_animate) { anim_in_cb(o, 256); return; }
    anim_in_cb(o, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_exec_cb(&a, anim_in_cb);
    lv_anim_set_values(&a, 0, 256);
    lv_anim_set_duration(&a, ANIM_MS);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

/* Animate `lbl` only if its text differs from what was last shown in it. */
typedef struct { lv_obj_t **lbl; char prev[BSD_SONG_TITLE_MAX + 8]; } watched_t;
static watched_t s_watch[] = {
    { &s_road_label, "" }, { &s_dir_caption, "" }, { &s_dir_label, "" },
    { &s_car_make, "" },   { &s_car_model, "" },
    { &s_artist, "" },     { &s_song, "" },
};

static bool animate_if_changed(lv_obj_t *lbl, uint32_t delay)
{
    for (size_t i = 0; i < sizeof(s_watch) / sizeof(s_watch[0]); i++) {
        if (*s_watch[i].lbl != lbl) continue;
        const char *now = lv_label_get_text(lbl);
        if (strcmp(now, s_watch[i].prev) == 0) return false;
        snprintf(s_watch[i].prev, sizeof(s_watch[i].prev), "%s", now);
        animate_in(lbl, delay);
        return true;
    }
    return false;
}

void bsd_ui_set_animations(bool on)
{
    s_animate = on;
    if (on) return;
    for (size_t i = 0; i < sizeof(s_watch) / sizeof(s_watch[0]); i++)
        if (*s_watch[i].lbl) animate_in(*s_watch[i].lbl, 0);   /* off: settle anything in flight */
}

/* ------------------------------------------------------------------ events */

static bool tap_allowed(void)
{
    uint32_t t = now_ms();
    if (t - s_last_advance_ms < TAP_GUARD_MS) return false;
    s_last_advance_ms = t;
    return true;
}

static void on_found(lv_event_t *e)
{
    (void)e;
    if (!tap_allowed()) return;
    uint32_t t = now_ms();
    const bsd_feed_t *f = bsd_feed_get();
    const char *road = (bsd_feed_road_freshness(t) == BSD_FRESH_OK) ? f->road.name : NULL;
    int before = bsd_drive_sighting_count();
    bsd_challenge_found(t, road);
    if (bsd_drive_sighting_count() > before) {          /* not recorded once the garage is full */
        const bsd_car_t *c = bsd_catalog_get(bsd_drive_sighting(before)->car_index);
        if (c) LV_LOG_USER("Found: %s %s (this drive: %d)", c->make, c->name, before + 1);
    }
}

static void on_skip(lv_event_t *e)
{
    (void)e;
    if (!tap_allowed()) return;
    bsd_challenge_skip();
}

static lv_obj_t *mk_button_ex(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                              const lv_font_t *font, const char *text, lv_event_cb_t cb, lv_obj_t **label_out)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);                         /* drops theme transitions */
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_ext_click_area(b, 4);                    /* a little extra for small fingers */
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *l = mk_label(b, font);
    lv_obj_set_width(l, w);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, text);
    place_text(l, 0, h / 2, false);
    *label_out = l;
    return b;
}

static lv_obj_t *mk_button(lv_obj_t *parent, int32_t y, int32_t h, const char *text, lv_event_cb_t cb, lv_obj_t **label_out)
{
    return mk_button_ex(parent, BTN_X, y, BTN_W, h, &bsd_font_label_20, text, cb, label_out);
}

/* Outlined button look (SKIP, GARAGE, BACK): texture shows through. */
static void style_outline_button(lv_obj_t *b, lv_obj_t *l, const theme_t *t)
{
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(b, t->skip_pr, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, t->skip_bd, 0);
    lv_obj_set_style_text_color(l, t->skip_tx, 0);
}

static void style_screen(lv_obj_t *scr, const theme_t *t)
{
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(t->base), 0);
    int ti = (t == theme_day()) ? 1 : 0;
    lv_obj_set_style_bg_image_src(scr, s_tile_px[ti] ? &s_tile[ti] : NULL, 0);
    lv_obj_set_style_bg_image_tiled(scr, true, 0);
}

/* ------------------------------------------------------------------ theme */

static void apply_theme(const theme_t *t)
{
    s_theme = t;
    style_screen(s_main_scr, t);
    style_screen(s_garage_scr, t);

    for (size_t i = 0; i < sizeof(s_rules) / sizeof(s_rules[0]); i++)
        lv_obj_set_style_bg_color(s_rules[i], t->line, 0);

    lv_obj_set_style_text_color(s_look, t->text, 0);

    lv_obj_set_style_bg_opa(s_btn_found, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_btn_found, t->found_bg, 0);
    lv_obj_set_style_bg_color(s_btn_found, t->found_pr, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(s_btn_found_l, t->found_tx, 0);

    style_outline_button(s_btn_skip, s_btn_skip_l, t);
    style_outline_button(s_btn_garage, s_btn_garage_l, t);

    /* everything state-dependent redraws with the new colors */
    s_seen_feed_rev = 0;
    s_seen_game_rev = 0;
    s_seen_road_fresh = s_seen_song_fresh = -1;
}

/* ------------------------------------------------------------------ garage */

typedef struct { int car; int times; const char *road; } garage_entry_t;

static int garage_entries(garage_entry_t *out, int max)
{
    int n = 0;
    for (int i = bsd_drive_sighting_count() - 1; i >= 0; i--) {     /* newest first */
        const bsd_sighting_t *sg = bsd_drive_sighting(i);
        int k = 0;
        while (k < n && out[k].car != sg->car_index) k++;
        if (k < n) { out[k].times++; continue; }
        if (n == max) break;
        out[n].car = sg->car_index;
        out[n].times = 1;
        out[n].road = sg->road;          /* most recent sighting's road */
        n++;
    }
    return n;
}

static lv_obj_t *s_back_btn, *s_back_l, *s_g_header;
static uint32_t  s_garage_touch_ms;      /* last press/scroll in the garage */

static void garage_touched(lv_event_t *e) { (void)e; s_garage_touch_ms = now_ms(); }

static void on_garage_back(lv_event_t *e);

/* Stack up to three single-line labels in a cell, centering their cap bands. */
static void stack_in_cell(lv_obj_t **lbls, int n, int32_t x, int32_t cell_h, int32_t gap)
{
    int32_t caps[3], total = gap * (n - 1);
    for (int i = 0; i < n; i++) { caps[i] = f_cap(lv_obj_get_style_text_font(lbls[i], 0)); total += caps[i]; }
    int32_t y = cell_h / 2 - total / 2;
    for (int i = 0; i < n; i++) {
        place_text(lbls[i], x, y + caps[i] / 2, true);
        y += caps[i] + gap;
    }
}

static void garage_build(void)
{
    static const lv_font_t *const make_f[]  = { &bsd_font_bold_25, &bsd_font_bold_20 };
    static const lv_font_t *const model_f[] = { &bsd_font_semi_20 };
    static const lv_font_t *const road_f[]  = { &bsd_font_label_18 };
    const theme_t *t = s_theme;
    lv_obj_t *scr = s_garage_scr;
    lv_obj_clean(scr);
    style_screen(scr, t);

    s_g_header = mk_label(scr, &bsd_font_label_20);
    lv_obj_set_style_text_color(s_g_header, t->text, 0);
    lv_label_set_text(s_g_header, "cars spotted this drive:");
    place_text(s_g_header, GRID_L, CY_HEADER, true);

    s_back_btn = mk_button_ex(scr, BTN_X, CY_HEADER - GBTN_H / 2, BTN_W, GBTN_H, &bsd_font_label_18,
                              "BACK", on_garage_back, &s_back_l);
    style_outline_button(s_back_btn, s_back_l, t);

    /* Roads in the list come from the feed, so demo data is labeled here too. */
    if (bsd_feed_get()->source == BSD_SRC_DEMO) {
        lv_obj_t *badge = lv_obj_create(scr);
        lv_obj_remove_style_all(badge);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(badge, C_DEMO, 0);
        lv_obj_set_style_radius(badge, 4, 0);
        lv_obj_set_style_pad_hor(badge, 8, 0);
        lv_obj_set_style_pad_ver(badge, 2, 0);
        lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_t *bl = mk_label(badge, &lv_font_montserrat_16);
        lv_obj_set_style_text_color(bl, C_DEMO_TX, 0);
        lv_label_set_text(bl, "DEMO DATA - not live");
        lv_obj_update_layout(badge);
        lv_obj_set_pos(badge, BTN_X - 12 - lv_obj_get_width(badge), CY_HEADER - lv_obj_get_height(badge) / 2);
    }

    lv_obj_t *rule = lv_obj_create(scr);
    lv_obj_remove_style_all(rule);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(rule, t->line, 0);
    lv_obj_set_pos(rule, GRID_L, ROW_ROAD_T);
    lv_obj_set_size(rule, GRID_W + 1, 1);

    /* scrollable table */
    s_garage_list = lv_obj_create(scr);
    lv_obj_remove_style_all(s_garage_list);
    lv_obj_set_pos(s_garage_list, GRID_L, ROW_ROAD_T + 1);
    lv_obj_set_size(s_garage_list, GRID_W + 1, SCR_H - ROW_ROAD_T - 1);
    lv_obj_set_scroll_dir(s_garage_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_garage_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(s_garage_list, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(s_garage_list, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(s_garage_list, t->skip_bd, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(s_garage_list, 0, LV_PART_SCROLLBAR);
    lv_obj_add_event_cb(s_garage_list, garage_touched, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_garage_list, garage_touched, LV_EVENT_SCROLL, NULL);

    static garage_entry_t e[BSD_DRIVE_MAX_SIGHTINGS];   /* static: too big for the LVGL task stack */
    int n = garage_entries(e, BSD_DRIVE_MAX_SIGHTINGS);
    if (n == 0) {
        lv_obj_t *l = mk_label(s_garage_list, &bsd_font_label_20);
        lv_obj_set_style_text_color(l, t->dim, 0);
        lv_obj_set_width(l, GRID_W);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(l, "No cars yet. Tap FOUND IT! when you spot one.");
        place_text(l, 0, (SCR_H - ROW_ROAD_T) / 2 - 20, false);
        return;
    }

    for (int i = 0; i < n; i++) {
        const bsd_car_t *c = bsd_catalog_get(e[i].car);
        int col = i % 2, row = i / 2;
        lv_obj_t *cell = lv_obj_create(s_garage_list);
        lv_obj_remove_style_all(cell);
        lv_obj_remove_flag(cell, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(cell, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_pos(cell, col * G_COL_W, row * G_ROW_H);
        lv_obj_set_size(cell, col ? GRID_W + 1 - G_COL_W : G_COL_W, G_ROW_H);
        lv_obj_set_style_border_color(cell, t->line, 0);
        lv_obj_set_style_border_width(cell, 1, 0);
        lv_obj_set_style_border_side(cell, (lv_border_side_t)(LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT |
                                     (col == 0 ? LV_BORDER_SIDE_LEFT : 0)), 0);
        int32_t tw = lv_obj_get_style_width(cell, 0) - 2 * G_TEXT_PAD;

        lv_obj_t *mk = mk_label(cell, make_f[0]);
        lv_obj_t *md = mk_label(cell, model_f[0]);
        lv_obj_t *rd = mk_label(cell, road_f[0]);
        lv_obj_set_style_text_color(mk, t->text, 0);
        lv_obj_set_style_text_color(md, t->text, 0);
        lv_obj_set_style_text_color(rd, t->dim, 0);
        fit_text(mk, c->make, make_f, 2, tw, 1);
        fit_text(md, c->name, model_f, 1, tw, 1);

        char where[BSD_SIGHTING_ROAD_MAX + 24];
        if (e[i].road && e[i].road[0])
        {
            if (e[i].times > 1) snprintf(where, sizeof(where), "seen %d times, last on %s", e[i].times, e[i].road);
            else                snprintf(where, sizeof(where), "on %s", e[i].road);
        }
        else
            snprintf(where, sizeof(where), e[i].times > 1 ? "seen %d times" : " ", e[i].times);
        fit_text(rd, where, road_f, 1, tw, 1);

        lv_obj_t *stack[3] = { mk, md, rd };
        stack_in_cell(stack, 3, G_TEXT_PAD, G_ROW_H, 10);
    }
}

static void garage_show(bool show)
{
    if (show) {
        s_garage_touch_ms = now_ms();
        garage_build();
        lv_screen_load(s_garage_scr);
    } else {
        lv_screen_load(s_main_scr);
    }
}

static void on_garage_open(lv_event_t *e)  { (void)e; garage_show(true); }
static void on_garage_back(lv_event_t *e)  { (void)e; garage_show(false); }

static void garage_idle_check(lv_timer_t *tm)
{
    (void)tm;
    if (lv_screen_active() == s_garage_scr && now_ms() - s_garage_touch_ms > GARAGE_IDLE_MS)
        garage_show(false);
}

bool bsd_ui_garage_visible(void) { return lv_screen_active() == s_garage_scr; }

/* ------------------------------------------------------------------ build */

void bsd_ui_create(bsd_clock_fn clock)
{
    s_clock = clock;
    make_tile(0, theme_night(), 0xA5F1A17u);
    make_tile(1, theme_day(),   0x5EED0DAu);

    lv_obj_t *scr = lv_screen_active();
    s_main_scr = scr;
    lv_obj_remove_style_all(scr);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_garage_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_garage_scr);
    lv_obj_remove_flag(s_garage_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_garage_scr, garage_touched, LV_EVENT_PRESSED, NULL);
    lv_timer_create(garage_idle_check, 1000, NULL);

    /* grid */
    static const int32_t r[8][4] = {
        { GRID_L, ROW_ROAD_T, GRID_W, 1 }, { GRID_L, ROW_CAR_T, GRID_W, 1 },
        { GRID_L, ROW_CAR_B, GRID_W, 1 },  { GRID_L, MUSIC_RULE, GRID_W, 1 },
        { DIR_X, ROW_ROAD_T, 1, ROW_CAR_T - ROW_ROAD_T }, { GRID_R, ROW_ROAD_T, 1, ROW_CAR_T - ROW_ROAD_T },
        { GRID_L, ROW_CAR_T, 1, ROW_CAR_B - ROW_CAR_T },  { LOOK_X, ROW_CAR_T, 1, ROW_CAR_B - ROW_CAR_T },
    };
    for (int i = 0; i < 8; i++) {
        lv_obj_t *o = lv_obj_create(scr);
        lv_obj_remove_style_all(o);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_pos(o, r[i][0], r[i][1]);
        lv_obj_set_size(o, r[i][2], r[i][3]);
        s_rules[i] = o;
    }

    /* header + status */
    s_header = mk_label(scr, &bsd_font_label_20);

    s_demo_badge = lv_obj_create(scr);
    lv_obj_remove_style_all(s_demo_badge);
    lv_obj_set_style_bg_opa(s_demo_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_demo_badge, C_DEMO, 0);
    lv_obj_set_style_radius(s_demo_badge, 4, 0);
    lv_obj_set_style_pad_hor(s_demo_badge, 8, 0);
    lv_obj_set_style_pad_ver(s_demo_badge, 2, 0);
    lv_obj_set_size(s_demo_badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_t *dl = mk_label(s_demo_badge, &lv_font_montserrat_16);
    lv_obj_set_style_text_color(dl, C_DEMO_TX, 0);
    lv_label_set_text(dl, "DEMO DATA - not live");

    s_link_label = mk_label(scr, &lv_font_montserrat_16);
    lv_obj_set_style_text_color(s_link_label, C_WARN, 0);

    /* road + direction */
    s_road_label  = mk_label(scr, &bsd_font_bold_40);
    s_dir_caption = mk_label(scr, &bsd_font_label_18);
    s_dir_label   = mk_label(scr, &bsd_font_bold_25);
    lv_obj_set_style_text_align(s_dir_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(s_dir_caption, LV_TEXT_ALIGN_CENTER, 0);

    /* "look out for a" */
    s_look = mk_label(scr, &bsd_font_label_18);
    lv_obj_set_width(s_look, LOOK_X - GRID_L - 2);
    lv_obj_set_style_text_align(s_look, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_look, "look\nout\nfor a");
    place_text(s_look, GRID_L + 1, CY_CAR, false);

    /* car */
    s_car_make  = mk_label(scr, &bsd_font_bold_35);
    s_car_model = mk_label(scr, &bsd_font_semi_25);
    s_btn_found = mk_button(scr, FOUND_Y, FOUND_H, "FOUND IT!", on_found, &s_btn_found_l);
    s_btn_skip  = mk_button(scr, SKIP_Y, SKIP_H, "SKIP", on_skip, &s_btn_skip_l);

    /* music */
    s_music_header = mk_label(scr, &bsd_font_label_20);
    lv_label_set_text(s_music_header, "Music Playing");
    place_text(s_music_header, GRID_L, CY_MUSIC_HD, true);
    s_artist = mk_label(scr, &bsd_font_bold_32);
    lv_label_set_long_mode(s_artist, LV_LABEL_LONG_DOT);
    s_song = mk_label(scr, &bsd_font_semi_24);
    lv_label_set_long_mode(s_song, LV_LABEL_LONG_DOT);

    s_btn_garage = mk_button_ex(scr, BTN_X, GBTN_Y, BTN_W, GBTN_H, &bsd_font_label_18, "GARAGE",
                                on_garage_open, &s_btn_garage_l);

    apply_theme(theme_night());
    bsd_ui_refresh();
}

/* ------------------------------------------------------------------ refresh */

static int32_t s_status_left = GRID_R;   /* leftmost x used by badge/link status */

static void draw_status(const bsd_feed_t *f)
{
    int32_t right = GRID_R;
    if (f->source == BSD_SRC_DEMO) {
        lv_obj_remove_flag(s_demo_badge, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(s_demo_badge);
        lv_obj_set_pos(s_demo_badge, right - lv_obj_get_width(s_demo_badge),
                       CY_HEADER - lv_obj_get_height(s_demo_badge) / 2);
        right -= lv_obj_get_width(s_demo_badge) + 12;
    } else {
        lv_obj_add_flag(s_demo_badge, LV_OBJ_FLAG_HIDDEN);
    }

    switch (f->link) {
    case BSD_LINK_CONNECTED:    lv_label_set_text(s_link_label, ""); break;
    case BSD_LINK_CONNECTING:   lv_label_set_text(s_link_label, LV_SYMBOL_BLUETOOTH " Connecting..."); break;
    case BSD_LINK_DISCONNECTED: lv_label_set_text(s_link_label, LV_SYMBOL_BLUETOOTH " Phone not connected"); break;
    }
    lv_obj_update_layout(s_link_label);
    int32_t lw = lv_obj_get_width(s_link_label);
    lv_obj_set_pos(s_link_label, right - lw, CY_HEADER - lv_obj_get_height(s_link_label) / 2);
    s_status_left = lv_label_get_text(s_link_label)[0] ? right - lw : right + 12;
}

static void draw_road_text(const bsd_feed_t *f, bsd_fresh_t fresh)
{
    static const lv_font_t *const road_fonts[] = { &bsd_font_bold_40, &bsd_font_bold_32, &bsd_font_bold_28 };
    static const lv_font_t *const dir_fonts[]  = { &bsd_font_bold_25, &bsd_font_bold_20 };
    const theme_t *t = s_theme;
    char buf[BSD_ROAD_NAME_MAX];

    bool lost = (fresh == BSD_FRESH_LOST), stale = (fresh == BSD_FRESH_STALE);
    lv_label_set_text(s_header, stale ? "last known road:" : "currently traveling on:");
    lv_obj_set_style_text_color(s_header, (lost || stale) ? t->stale : t->text, 0);
    place_text(s_header, GRID_L, CY_HEADER, true);

    if (lost) {
        lv_obj_set_style_text_color(s_road_label, t->stale, 0);
        fit_text(s_road_label, "FINDING THE ROAD...", road_fonts + 2, 1, ROAD_TEXT_W, 1);
        place_text(s_road_label, ROAD_TEXT_X, CY_ROAD, true);
        lv_label_set_text(s_dir_label, "");
        lv_label_set_text(s_dir_caption, "");
        return;
    }

    to_upper(buf, sizeof(buf), f->road.name);
    lv_obj_set_style_text_color(s_road_label, stale ? t->stale : t->text, 0);
    fit_text(s_road_label, buf, road_fonts, 3, ROAD_TEXT_W, 2);
    place_text(s_road_label, ROAD_TEXT_X, CY_ROAD, true);

    /* Direction cell. Signed route direction ("WEST") as in the Figma. A
     * travel bearing is a different fact: dimmer, with a "heading" caption,
     * so it never reads as the posted route direction. */
    const int32_t dir_x = (DIR_X + GRID_R) / 2 - DIR_W / 2;
    to_upper(buf, sizeof(buf), f->road.direction);
    if (f->road.dir_kind == BSD_DIR_SIGNED) {
        lv_obj_set_style_text_color(s_dir_label, stale ? t->stale : t->text, 0);
        fit_text(s_dir_label, buf, dir_fonts, 2, DIR_W, 1);
        lv_label_set_text(s_dir_caption, "");
        place_text(s_dir_label, dir_x, CY_ROAD, false);
    } else if (f->road.dir_kind == BSD_DIR_BEARING) {
        lv_obj_set_style_text_color(s_dir_label, stale ? t->stale : t->dim, 0);
        lv_obj_set_style_text_color(s_dir_caption, stale ? t->stale : t->dim, 0);
        fit_text(s_dir_label, buf, dir_fonts + 1, 1, DIR_W, 1);
        lv_obj_set_width(s_dir_caption, DIR_W);
        lv_label_set_text(s_dir_caption, "heading");
        /* stack: caption cap band, 10 px gap, direction cap band; center the pair */
        lv_obj_update_layout(s_dir_caption);
        int32_t c_cap = f_cap(&bsd_font_label_18), d_cap = f_cap(lv_obj_get_style_text_font(s_dir_label, 0));
        int32_t total = c_cap + 10 + d_cap, top = CY_ROAD - total / 2;
        place_text(s_dir_caption, dir_x, top + c_cap / 2, false);
        place_text(s_dir_label, dir_x, top + c_cap + 10 + d_cap / 2, false);
    } else {
        lv_label_set_text(s_dir_label, "");
        lv_label_set_text(s_dir_caption, "");
    }
}

static void draw_road(const bsd_feed_t *f, bsd_fresh_t fresh)
{
    draw_road_text(f, fresh);
    bool road = animate_if_changed(s_road_label, 0);
    animate_if_changed(s_dir_caption, road ? ANIM_STAGGER : 0);
    animate_if_changed(s_dir_label, road ? ANIM_STAGGER : 0);
}

static void draw_song_text(const bsd_feed_t *f, bsd_fresh_t fresh)
{
    const theme_t *t = s_theme;
    bool show = (fresh != BSD_FRESH_LOST) && f->song.valid;
    /* Don't claim music is playing when we don't know of any. */
    lv_label_set_text(s_music_header, show ? "Music Playing" : "No music playing");
    place_text(s_music_header, GRID_L, CY_MUSIC_HD, true);
    lv_obj_set_style_text_color(s_music_header, show ? t->text : t->stale, 0);
    lv_obj_set_style_text_color(s_artist, t->text, 0);
    lv_obj_set_style_text_color(s_song, t->text, 0);
    if (!show) {
        lv_label_set_text(s_artist, "");
        lv_label_set_text(s_song, "");
        return;
    }

    /* Artist (bold) then song on one shared baseline, as in the Figma. Long
     * artists are capped so the song keeps room; both truncate with "...". */
    const lv_font_t *af = &bsd_font_bold_32, *sf = &bsd_font_semi_24;
    lv_point_t sz;
    int32_t sx = GRID_L;
    int32_t baseline = CY_MUSIC + f_cap(af) / 2;   /* used when there is no artist */
    if (f->song.artist[0]) {
        lv_text_get_size(&sz, f->song.artist, af, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        int32_t aw = sz.x < ARTIST_MAX_W ? sz.x : ARTIST_MAX_W;
        lv_obj_set_size(s_artist, aw, af->line_height);
        lv_label_set_text(s_artist, f->song.artist);
        baseline = place_text(s_artist, GRID_L, CY_MUSIC, true) + f_ascent(af);
        sx = GRID_L + aw + ARTIST_SONG_GAP;
    } else {
        lv_label_set_text(s_artist, "");
    }
    lv_obj_set_size(s_song, GRID_R - sx, sf->line_height);
    lv_label_set_text(s_song, f->song.title);
    lv_obj_update_layout(s_song);
    lv_obj_set_pos(s_song, sx - first_bearing(s_song), baseline - f_ascent(sf));
}

static void draw_song(const bsd_feed_t *f, bsd_fresh_t fresh)
{
    draw_song_text(f, fresh);
    bool artist = animate_if_changed(s_artist, 0);
    animate_if_changed(s_song, artist ? ANIM_STAGGER : 0);
}

static void draw_car_text(const char *make, const char *name)
{
    static const lv_font_t *const make_fonts[]  = { &bsd_font_bold_35, &bsd_font_bold_28 };
    static const lv_font_t *const model_fonts[] = { &bsd_font_semi_25, &bsd_font_semi_20 };
    const theme_t *t = s_theme;

    lv_obj_set_style_text_color(s_car_make, t->text, 0);
    lv_obj_set_style_text_color(s_car_model, t->text, 0);
    fit_text(s_car_make, make, make_fonts, 2, CAR_TEXT_W, 1);
    fit_text(s_car_model, name, model_fonts, 2, CAR_TEXT_W, 2);

    /* Center the block "make cap top .. model last baseline" in the car row. */
    int32_t make_cap = f_cap(lv_obj_get_style_text_font(s_car_make, 0));
    int32_t model_top = ink_top_off(s_car_model), model_base = ink_base_off(s_car_model);
    int32_t total = make_cap + MAKE_MODEL_GAP + (model_base - model_top);
    int32_t top = CY_CAR - total / 2;
    place_text(s_car_make, CAR_TEXT_X, top + make_cap / 2, true);
    lv_obj_update_layout(s_car_model);
    lv_obj_set_pos(s_car_model, CAR_TEXT_X - first_bearing(s_car_model),
                   top + make_cap + MAKE_MODEL_GAP - model_top);

    /* Same make (e.g. Ford -> Ford): only the model animates. */
    bool m = animate_if_changed(s_car_make, 0);
    animate_if_changed(s_car_model, m ? ANIM_STAGGER : 0);
}

static void draw_car(void)
{
    if (s_dbg_make) { draw_car_text(s_dbg_make, s_dbg_name); return; }
    const bsd_car_t *c = bsd_catalog_get(bsd_challenge_current());
    if (c) draw_car_text(c->make, c->name);
}

void bsd_ui_refresh(void)
{
    const bsd_feed_t *f = bsd_feed_get();
    const theme_t *want = (f->light == BSD_LIGHT_DAY) ? theme_day() : theme_night();
    if (want != s_theme) {
        apply_theme(want);
        if (bsd_ui_garage_visible()) garage_build();
    }

    uint32_t t = now_ms();
    bsd_fresh_t road_fresh = bsd_feed_road_freshness(t);
    bsd_fresh_t song_fresh = bsd_feed_song_freshness(t);

    bool feed_changed = f->revision != s_seen_feed_rev;
    if (feed_changed) draw_status(f);
    if (feed_changed || (int)road_fresh != s_seen_road_fresh) draw_road(f, road_fresh);
    if (feed_changed || (int)song_fresh != s_seen_song_fresh) draw_song(f, song_fresh);
    s_seen_feed_rev = f->revision;
    s_seen_road_fresh = (int)road_fresh;
    s_seen_song_fresh = (int)song_fresh;

    if (bsd_game_revision() != s_seen_game_rev) {
        s_seen_game_rev = bsd_game_revision();
        draw_car();
    }
}

/* ------------------------------------------------------------------ test hooks */

void bsd_ui_debug_show_car(const char *make, const char *name)
{
    s_dbg_make = make;
    s_dbg_name = name;
    s_seen_game_rev = 0;
}

#ifdef BSD_UI_TEST_HOOKS   /* layout checker: simulator builds only */

/* Ink box of a label: letters' left/right extent and cap-top .. descender. */
static lv_area_t ink_box(lv_obj_t *lbl)
{
    lv_area_t a;
    lv_obj_update_layout(lbl);
    lv_obj_get_coords(lbl, &a);
    const lv_font_t *f = lv_obj_get_style_text_font(lbl, 0);
    int32_t ls = lv_obj_get_style_text_line_space(lbl, 0);
    int32_t w = lv_obj_get_width(lbl);
    lv_point_t sz;
    lv_text_get_size(&sz, lv_label_get_text(lbl), f, 0, ls, w, LV_TEXT_FLAG_NONE);
    lv_area_t ink;
    lv_text_align_t al = lv_obj_get_style_text_align(lbl, 0);
    int32_t text_w = sz.x < w || lv_label_get_long_mode(lbl) == LV_LABEL_LONG_DOT ? (sz.x < w ? sz.x : w) : sz.x;
    ink.x1 = (al == LV_TEXT_ALIGN_CENTER) ? a.x1 + (w - text_w) / 2 : a.x1 + first_bearing(lbl);
    ink.x2 = ink.x1 + text_w - 1 - ((al == LV_TEXT_ALIGN_CENTER) ? 0 : first_bearing(lbl));
    ink.y1 = a.y1 + ink_top_off(lbl);
    ink.y2 = a.y1 + ink_base_off(lbl) + f->base_line;   /* include descender room */
    return ink;
}

static int check(const char *what, lv_obj_t *lbl, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    if (!lbl || lv_obj_has_flag(lbl, LV_OBJ_FLAG_HIDDEN)) return 0;
    const char *t = lv_label_get_text(lbl);
    if (!t || !t[0]) return 0;
    lv_area_t k = ink_box(lbl);
    if (k.x1 < x1 || k.y1 < y1 || k.x2 > x2 || k.y2 > y2) {
        printf("  SPILL %-12s ink (%d,%d)-(%d,%d) outside cell (%d,%d)-(%d,%d): \"%s\"\n", what,
               (int)k.x1, (int)k.y1, (int)k.x2, (int)k.y2, (int)x1, (int)y1, (int)x2, (int)y2, t);
        return 1;
    }
    return 0;
}

static int check_garage(void)
{
    int bad = 0;
    bad += check("g-header", s_g_header, GRID_L, 0, BTN_X - 8, ROW_ROAD_T - 1);
    uint32_t n = lv_obj_get_child_count(s_garage_list);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *cell = lv_obj_get_child(s_garage_list, (int32_t)i);
        lv_area_t a;
        lv_obj_get_coords(cell, &a);
        if (lv_obj_check_type(cell, &lv_label_class)) {           /* empty-state message */
            bad += check("g-empty", cell, GRID_L, ROW_ROAD_T + 1, GRID_R, SCR_H - 1);
            continue;
        }
        static const char *names[] = { "g-make", "g-model", "g-road" };
        for (uint32_t k = 0; k < lv_obj_get_child_count(cell) && k < 3; k++)
            bad += check(names[k], lv_obj_get_child(cell, (int32_t)k),
                         a.x1 + G_TEXT_PAD, a.y1 + 1, a.x2 - G_TEXT_PAD / 2, a.y2 - 1);
    }
    return bad;
}

int bsd_ui_debug_check_layout(void)
{
    if (bsd_ui_garage_visible()) { lv_obj_update_layout(s_garage_scr); return check_garage(); }
    int bad = 0;
    bad += check("header",  s_header,      GRID_L, 0, s_status_left - 8, ROW_ROAD_T - 1);
    bad += check("road",    s_road_label,  GRID_L, ROW_ROAD_T + 1, DIR_X - 6, ROW_CAR_T - 1);
    bad += check("dir",     s_dir_label,   DIR_X + 2, ROW_ROAD_T + 1, GRID_R - 2, ROW_CAR_T - 1);
    bad += check("dir-cap", s_dir_caption, DIR_X + 2, ROW_ROAD_T + 1, GRID_R - 2, ROW_CAR_T - 1);
    bad += check("look",    s_look,        GRID_L + 1, ROW_CAR_T + 1, LOOK_X - 1, ROW_CAR_B - 1);
    bad += check("make",    s_car_make,    CAR_TEXT_X, ROW_CAR_T + 1, BTN_X - 6, ROW_CAR_B - 1);
    bad += check("model",   s_car_model,   CAR_TEXT_X, ROW_CAR_T + 1, BTN_X - 6, ROW_CAR_B - 1);
    bad += check("music-hd", s_music_header, GRID_L, ROW_CAR_B + 1, BTN_X - 6, MUSIC_RULE - 1);
    bad += check("artist",  s_artist,      GRID_L, MUSIC_RULE + 1, GRID_R, SCR_H - 1);
    bad += check("song",    s_song,        GRID_L, MUSIC_RULE + 1, GRID_R, SCR_H - 1);
    if (s_artist && lv_label_get_text(s_artist)[0] && lv_label_get_text(s_song)[0]) {
        lv_area_t a = ink_box(s_artist), s = ink_box(s_song);
        if (a.x2 + 16 > s.x1) { printf("  SPILL artist runs into song\n"); bad++; }
    }
    return bad;
}

#endif /* BSD_UI_TEST_HOOKS */
