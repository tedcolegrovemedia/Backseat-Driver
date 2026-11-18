/*
 * Desktop simulator: 800x480 SDL window, mouse = touch.
 *
 *   ./backseat_sim                 interactive, demo replay running
 *   ./backseat_sim --shots DIR     render each display state to DIR as .bmp files and exit
 *
 * Developer keys (simulator only — never part of the child-facing UI):
 *   n  next demo step         p  pause/resume demo
 *   d  toggle phone link      r  drop location      m  drop song
 *   f  Found it               s  Skip
 *   g  print This Drive garage to the console
 *   x  start a new drive      o  demo off / on
 *   t  toggle day / night look   a  text animation on / off
 */
#include "bsd_cars.h"
#include "bsd_demo.h"
#include "bsd_feed.h"
#include "bsd_link.h"
#include "bsd_ui.h"

#include "lvgl.h"
#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static bool     s_fake_clock;
static uint32_t s_fake_now;

static bool     s_anim = true;

static uint32_t sim_clock(void) { return s_fake_clock ? s_fake_now : SDL_GetTicks(); }

/* ------------------------------------------------------------ garage print */

static void print_garage(void)
{
    int n = bsd_drive_sighting_count();
    printf("\n=== This Drive: %d car%s spotted ===\n", n, n == 1 ? "" : "s");
    for (int i = 0; i < n; i++) {
        const bsd_sighting_t *s = bsd_drive_sighting(i);
        const bsd_car_t *c = bsd_catalog_get(s->car_index);
        printf("  %2d. %-12s %-28s  +%02u:%02u  %s\n", i + 1, c->make, c->name,
               (unsigned)(s->at_ms / 60000), (unsigned)(s->at_ms / 1000 % 60),
               s->road[0] ? s->road : "(road unknown)");
    }
    printf("\n");
    fflush(stdout);
}

/* ------------------------------------------------------------ dev keys */

static void on_key(lv_event_t *e)
{
    uint32_t k = lv_event_get_key(e);
    uint32_t t = sim_clock();
    const bsd_feed_t *f = bsd_feed_get();
    switch (k) {
    case 'n': bsd_demo_next(t); printf("[demo] %s\n", bsd_demo_step_label()); break;
    case 'p': bsd_demo_set_paused(!bsd_demo_paused()); printf("[demo] %s\n", bsd_demo_paused() ? "paused" : "running"); break;
    case 'd': bsd_feed_set_link(f->link == BSD_LINK_CONNECTED ? BSD_LINK_DISCONNECTED : BSD_LINK_CONNECTED); break;
    case 'r': bsd_feed_clear_road(); break;
    case 'm': bsd_feed_clear_song(); break;
    case 'f': bsd_challenge_found(t, bsd_feed_road_freshness(t) == BSD_FRESH_OK ? f->road.name : NULL); break;
    case 's': bsd_challenge_skip(); break;
    case 'g': print_garage(); break;
    case 'a': s_anim = !s_anim; bsd_ui_set_animations(s_anim); printf("[ui] text animation %s\n", s_anim ? "on" : "off"); break;
    case 't': bsd_feed_set_light(f->light == BSD_LIGHT_DAY ? BSD_LIGHT_NIGHT : BSD_LIGHT_DAY); break;
    case 'x': print_garage(); bsd_drive_start(t); printf("[drive] new drive started\n"); break;
    case 'o':
        if (bsd_demo_running()) { bsd_demo_stop(); bsd_feed_clear_road(); bsd_feed_clear_song(); bsd_feed_set_link(BSD_LINK_DISCONNECTED); }
        else bsd_demo_start(t);
        printf("[demo] %s\n", bsd_demo_running() ? "on" : "off");
        break;
    default: break;
    }
    fflush(stdout);
}

static void install_dev_keys(void)
{
    lv_indev_t *kb = lv_sdl_keyboard_create();
    lv_group_t *g = lv_group_create();
    lv_indev_set_group(kb, g);
    lv_obj_t *sink = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(sink);
    lv_obj_set_size(sink, 1, 1);
    lv_obj_add_event_cb(sink, on_key, LV_EVENT_KEY, NULL);
    lv_group_add_obj(g, sink);
    lv_group_focus_obj(sink);
}

/* ------------------------------------------------------------ screenshots */

static int write_bmp(const char *path, lv_draw_buf_t *buf)
{
    int w = buf->header.w, h = buf->header.h, stride = buf->header.stride;
    int row = (w * 3 + 3) & ~3;
    uint32_t size = 54 + (uint32_t)row * h;
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    uint8_t hdr[54] = { 'B', 'M' };
    memcpy(hdr + 2, &size, 4);
    hdr[10] = 54; hdr[14] = 40;
    memcpy(hdr + 18, &w, 4); memcpy(hdr + 22, &h, 4);
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, 54, fp);
    uint8_t *line = calloc(1, row);
    for (int y = h - 1; y >= 0; y--) {               /* BMP is bottom-up, BGR */
        const uint16_t *px = (const uint16_t *)(buf->data + y * stride); /* RGB565 */
        for (int x = 0; x < w; x++) {
            uint16_t c = px[x];
            line[x * 3 + 0] = (uint8_t)((c & 0x1F) << 3);
            line[x * 3 + 1] = (uint8_t)(((c >> 5) & 0x3F) << 2);
            line[x * 3 + 2] = (uint8_t)(((c >> 11) & 0x1F) << 3);
        }
        fwrite(line, 1, row, fp);
    }
    free(line);
    fclose(fp);
    return 0;
}

static int s_spills;

static void shot(const char *dir, const char *name)
{
    char path[512];
    s_fake_now += 10;
    bsd_ui_refresh();
    lv_obj_update_layout(lv_screen_active());
    int sp = bsd_ui_debug_check_layout();
    if (sp) { printf("  ^ %d spill(s) in %s\n", sp, name); s_spills += sp; }
    lv_draw_buf_t *b = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
    if (!b) { fprintf(stderr, "snapshot failed: %s\n", name); return; }
    /* The background must be textured, not flat: look at a strip in the
     * left margin, which never holds text. */
    {
        const uint16_t *row0 = (const uint16_t *)(b->data + 240 * b->header.stride);
        bool varied = false;
        for (int y = 200; y < 300 && !varied; y++) {
            const uint16_t *px = (const uint16_t *)(b->data + y * b->header.stride);
            for (int x = 2; x < 22; x++) if (px[x] != row0[2]) { varied = true; break; }
        }
        if (!varied) { printf("  FLAT BACKGROUND in %s (texture missing)\n", name); s_spills++; }
    }
    snprintf(path, sizeof(path), "%s/%s.bmp", dir, name);
    write_bmp(path, b);
    lv_draw_buf_destroy(b);
}

/* ------------------------------------------------------------ virtual touch
 * Drives real taps through LVGL's input pipeline so Found it / Skip are
 * tested via the actual buttons (hit-testing, CLICKED event, tap guard). */

static lv_indev_t *s_vtouch;
static lv_point_t  s_vt_point;
static bool        s_vt_pressed;

static void vtouch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point = s_vt_point;
    data->state = s_vt_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void vtap(int32_t x, int32_t y)
{
    s_vt_point.x = x; s_vt_point.y = y;
    s_vt_pressed = true;  lv_indev_read(s_vtouch); s_fake_now += 60;
    s_vt_pressed = false; lv_indev_read(s_vtouch); s_fake_now += 40;
}

static int s_failures;
static const int32_t found_x_main = 697, found_y_main = 234;
#define CHECK(cond, msg) do { if (cond) printf("  ok   %s\n", msg); \
                              else { printf("  FAIL %s\n", msg); s_failures++; } } while (0)

static void run_link_tests(void)
{
    printf("phone link messages:\n");
    const bsd_feed_t *f = bsd_feed_get();
    uint32_t t = s_fake_now;
#define LINK(m) bsd_link_apply(m, strlen(m), t)
    CHECK(LINK("R\x1fS\x1fWest\x1fRoute 22") && f->road.valid &&
          strcmp(f->road.name, "Route 22") == 0 && f->road.dir_kind == BSD_DIR_SIGNED &&
          strcmp(f->road.direction, "West") == 0, "signed road");
    CHECK(LINK("R\x1f" "B\x1fNorthwest\x1fHamilton Blvd") &&
          f->road.dir_kind == BSD_DIR_BEARING, "bearing road");
    CHECK(LINK("R\x1fN\x1f\x1f" "Cedar Crest") && f->road.dir_kind == BSD_DIR_NONE, "road without direction");
    CHECK(LINK("r") && !f->road.valid, "road cleared");
    CHECK(LINK("S\x1f" "1\x1fHey | Jude\x1fThe Beatles") &&
          strcmp(f->song.title, "Hey | Jude") == 0 && strcmp(f->song.artist, "The Beatles") == 0 &&
          f->song.playing, "song with a | in the title");
    CHECK(LINK("s") && !f->song.valid, "song cleared");
    CHECK(LINK("L\x1f" "d") && f->light == BSD_LIGHT_DAY, "day");
    CHECK(LINK("L\x1fn") && f->light == BSD_LIGHT_NIGHT, "night");
    uint32_t rev = f->revision;
    CHECK(!LINK("R\x1fS\x1fWest") && f->revision == rev, "short road message rejected");
    CHECK(!LINK("X") && !LINK("") && !LINK("L\x1fq"), "unknown messages rejected");
    CHECK(LINK("S\x1f" "1\x1f" "Caf\xc3\xa9\x1f" "A\x07" "B") &&
          strcmp(f->song.title, "Caf  ") == 0 && strcmp(f->song.artist, "A B") == 0, "non-ASCII bytes become spaces");
    char big[1000]; memset(big, 'x', sizeof big); big[0] = 'R'; big[1] = 0x1f; big[2] = 'S'; big[3] = 0x1f; big[4] = 0x1f;
    CHECK(bsd_link_apply(big, sizeof big, t) && strlen(f->road.name) == BSD_ROAD_NAME_MAX - 1, "oversized message is truncated safely");
    bsd_feed_clear_road(); bsd_feed_clear_song();
#undef LINK
}

static void run_touch_tests(void)
{
    s_vtouch = lv_indev_create();
    lv_indev_set_type(s_vtouch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_vtouch, vtouch_read);
    lv_timer_pause(lv_indev_get_read_timer(s_vtouch));  /* we read it by hand */

    const int32_t found_x = 697, found_y = 234, skip_x = 697, skip_y = 304;
    /* edges: a tap 3 px inside each button's top and bottom must still land */
    const int32_t found_top = 199, found_bot = 268, skip_top = 283, skip_bot = 324;
    printf("\ntouch tests:\n");

    s_fake_now += 2000;
    int before = bsd_drive_sighting_count();
    int car = bsd_challenge_current();
    vtap(found_x, found_y);
    CHECK(bsd_drive_sighting_count() == before + 1, "Found it tap records a sighting");
    CHECK(bsd_drive_sighting(before)->car_index == car, "sighting is the car that was shown");
    CHECK(bsd_challenge_current() != car, "Found it advances to another car");

    int after_first = bsd_drive_sighting_count();
    vtap(found_x, found_y);                       /* double-tap inside guard window */
    CHECK(bsd_drive_sighting_count() == after_first, "double-tap within 700 ms is ignored");

    s_fake_now += 1000;
    car = bsd_challenge_current();
    vtap(skip_x, skip_y);
    CHECK(bsd_drive_sighting_count() == after_first, "Skip does not record a sighting");
    CHECK(bsd_challenge_current() != car, "Skip advances to another car");

    s_fake_now += 1000;
    int n = bsd_drive_sighting_count();
    vtap(400, 60);                                 /* tap on the road text */
    CHECK(bsd_drive_sighting_count() == n, "taps outside the buttons do nothing");

    s_fake_now += 1000; n = bsd_drive_sighting_count();
    vtap(found_x, found_top);
    CHECK(bsd_drive_sighting_count() == n + 1, "top edge of the taller FOUND IT! counts");
    s_fake_now += 1000; n = bsd_drive_sighting_count();
    vtap(found_x, found_bot);
    CHECK(bsd_drive_sighting_count() == n + 1, "bottom edge of FOUND IT! counts");
    s_fake_now += 1000; n = bsd_drive_sighting_count(); car = bsd_challenge_current();
    vtap(skip_x, skip_top);
    CHECK(bsd_drive_sighting_count() == n && bsd_challenge_current() != car, "top edge of SKIP skips (not found)");
    s_fake_now += 1000; car = bsd_challenge_current();
    vtap(skip_x, skip_bot);
    CHECK(bsd_challenge_current() != car, "bottom edge of SKIP skips");

    /* Picker mix over many skips: rare cars appear, never twice in a row. */
    int tiers[BSD_RARITY_COUNT] = {0}, rare_runs = 0, prev_rare = 0;
    for (int i = 0; i < 2000; i++) {
        bsd_challenge_skip();
        int r = bsd_catalog_get(bsd_challenge_current())->rarity;
        tiers[r]++;
        if (r == BSD_RARE && prev_rare) rare_runs++;
        prev_rare = (r == BSD_RARE);
    }
    printf("  mix over 2000 picks: common %d, uncommon %d, rare %d\n",
           tiers[BSD_COMMON], tiers[BSD_UNCOMMON], tiers[BSD_RARE]);
    CHECK(tiers[BSD_RARE] > 0 && tiers[BSD_COMMON] > tiers[BSD_UNCOMMON], "tier mix follows weights");
    CHECK(rare_runs == 0, "never two rare cars in a row");
    printf("%s\n", s_failures ? "TOUCH TESTS FAILED" : "all touch tests passed");
}

static void run_garage_tests(const char *dir)
{
    const int32_t garage_x = 697, garage_y = 361, back_x = 697, back_y = 28;
    printf("\ngarage tests:\n");
    bsd_feed_set_light(BSD_LIGHT_NIGHT);
    bsd_feed_set_road("Route 22", BSD_DIR_SIGNED, "West", s_fake_now);

    bsd_drive_start(s_fake_now);
    bsd_ui_refresh();
    s_fake_now += 1000;
    vtap(garage_x, garage_y);
    CHECK(bsd_ui_garage_visible(), "GARAGE button opens the garage");
    shot(dir, "17_garage_empty");
    vtap(found_x_main, found_y_main);
    CHECK(bsd_drive_sighting_count() == 0, "FOUND IT! area does nothing while the garage is open");
    vtap(back_x, back_y);
    CHECK(!bsd_ui_garage_visible(), "BACK returns to the main screen");

    /* a few finds on different roads, one car twice */
    static const char *roads[] = { "Route 22", "Route 378", "Hamilton Boulevard", "Interstate 78", "Route 22" };
    for (int i = 0; i < 5; i++) {
        bsd_feed_set_road(roads[i], BSD_DIR_SIGNED, "West", s_fake_now);
        bsd_challenge_found(s_fake_now, roads[i]);
        s_fake_now += 60000;
    }
    bsd_ui_refresh();
    s_fake_now += 1000;
    vtap(garage_x, garage_y);
    CHECK(bsd_ui_garage_visible(), "garage opens with cars in it");
    shot(dir, "18_garage_night");
    bsd_feed_set_light(BSD_LIGHT_DAY);
    shot(dir, "19_garage_day");
    CHECK(bsd_ui_garage_visible(), "switching to day keeps the garage open");

    /* idle auto-return */
    for (int i = 0; i < 500; i++) { s_fake_now += 100; lv_timer_handler(); }
    CHECK(!bsd_ui_garage_visible(), "garage returns to the main screen after 45 s idle");

    /* every catalog car: scrolling list, long names */
    bsd_feed_set_light(BSD_LIGHT_NIGHT);
    for (int i = 0; i < 400 && bsd_drive_sighting_count() < 60; i++) {
        bsd_feed_set_road("Northeast Extension of the Pennsylvania Turnpike", BSD_DIR_SIGNED, "North", s_fake_now);
        bsd_challenge_found(s_fake_now, "Northeast Extension of the Pennsylvania Turnpike");
    }
    bsd_ui_refresh();
    s_fake_now += 1000;
    vtap(garage_x, garage_y);
    shot(dir, "20_garage_full");
    vtap(back_x, back_y);
    CHECK(!bsd_ui_garage_visible(), "BACK works with a full garage");
    bsd_ui_refresh();
}

static int run_shots(const char *dir)
{
    s_fake_clock = true;
    s_fake_now = 1000;
    bsd_feed_init();
    bsd_game_init(NULL, 42, s_fake_now);
    bsd_ui_create(sim_clock);
    bsd_ui_set_animations(false);   /* snapshots must show settled text */
    bsd_feed_set_source(BSD_SRC_DEMO);
    bsd_feed_set_link(BSD_LINK_CONNECTED);

    bsd_feed_set_road("Route 22", BSD_DIR_SIGNED, "West", s_fake_now);
    bsd_feed_set_song("Here Comes the Sun", "The Beatles", true, s_fake_now);
    shot(dir, "01_signed_direction");

    bsd_feed_set_road("Hamilton Boulevard", BSD_DIR_BEARING, "Southwest", s_fake_now);
    shot(dir, "02_bearing_only");

    bsd_feed_set_road("Northeast Extension of the Pennsylvania Turnpike", BSD_DIR_SIGNED, "North", s_fake_now);
    bsd_feed_set_song("The Longest Song Title We Could Think Of For Testing (Extended Live Version)",
                      "The Lehigh Valley Philharmonic Orchestra and Friends", true, s_fake_now);
    while (strcmp(bsd_catalog_get(bsd_challenge_current())->id, "mercedes_amg_g63") != 0) bsd_challenge_skip();
    shot(dir, "03_long_names");

    bsd_feed_set_road("Route 378", BSD_DIR_SIGNED, "North", s_fake_now);
    bsd_challenge_skip();
    s_fake_now += BSD_ROAD_STALE_MS + 1000;
    bsd_feed_set_song("Drive My Car", "The Beatles", true, s_fake_now);
    shot(dir, "04_stale_location");

    s_fake_now += BSD_ROAD_LOST_MS;
    bsd_feed_clear_song();
    shot(dir, "05_no_location_no_song");

    bsd_feed_set_link(BSD_LINK_DISCONNECTED);
    shot(dir, "06_phone_disconnected");

    bsd_feed_set_source(BSD_SRC_LIVE);
    bsd_feed_set_link(BSD_LINK_CONNECTED);
    bsd_feed_set_road("Interstate 78", BSD_DIR_SIGNED, "East", s_fake_now);
    bsd_feed_set_song("Bluebird", "Alexa Rose", true, s_fake_now);
    bsd_challenge_found(s_fake_now, "Interstate 78");
    shot(dir, "07_live_source_no_badge");

    /* ---- day theme ---- */
    bsd_feed_set_light(BSD_LIGHT_DAY);
    bsd_feed_set_source(BSD_SRC_DEMO);
    bsd_feed_set_road("Route 22", BSD_DIR_SIGNED, "West", s_fake_now);
    bsd_feed_set_song("Here Comes the Sun", "The Beatles", true, s_fake_now);
    shot(dir, "08_day_signed");
    bsd_feed_set_road("Hamilton Boulevard", BSD_DIR_BEARING, "Southwest", s_fake_now);
    bsd_feed_set_link(BSD_LINK_DISCONNECTED);
    shot(dir, "09_day_bearing_disconnected");
    bsd_feed_set_link(BSD_LINK_CONNECTED);
    s_fake_now += BSD_ROAD_STALE_MS + 1000;
    bsd_feed_set_song("Drive My Car", "The Beatles", true, s_fake_now);
    shot(dir, "10_day_stale");

    /* ---- stress: longest plausible and absurd strings, both themes ---- */
    printf("\nlayout stress:\n");
    static const struct {
        const char *name, *road, *dir; bsd_dir_kind_t kind;
        const char *make, *model, *artist, *song;
    } stress[] = {
        { "11_stress_wide_letters",
          "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW", "North", BSD_DIR_SIGNED,
          "WWWWWWWWWWWWWWWWWWWW", "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW",
          "WWWWWWWWWWWWWWWWWWWWWWWW", "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW" },
        { "12_stress_one_long_word",
          "Llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch", "Northwest", BSD_DIR_BEARING,
          "Supercalifragilisticexpialidocious", "Pneumonoultramicroscopicsilicovolcanoconiosis",
          "Wolfeschlegelsteinhausenbergerdorff", "Hippopotomonstrosesquippedaliophobia" },
        { "13_stress_real_long",
          "Lehigh Valley Thruway Northeast Extension Connector to Interstate 476 Toll Plaza", "South", BSD_DIR_SIGNED,
          "Mercedes-Benz AMG Performance", "G 63 4MATIC Professional Grand Edition Night Package",
          "Florence + the Machine featuring Kendrick Lamar", "The Dog Days Are Over (Live at the Royal Albert Hall)" },
        { "14_stress_max_buffers",
          "Aaaaaaaaaa Bbbbbbbbbb Cccccccccc Dddddddddd Eeeeeeeeee Ffffffffff Gggggggggg Hhhhhhhhhh Iiiiiiiiii",
          "Southwest", BSD_DIR_BEARING,
          "Aston Martin Lagonda Limited of Gaydon", "Valkyrie AMR Pro Limited Edition Track-Only Hypercar Prototype",
          "", "An instrumental track with no artist and a title that goes on and on and on and on forever" },
        { "15_stress_short",
          "PA 9", "East", BSD_DIR_SIGNED, "Kia", "K5", "U2", "One" },
    };
    for (int theme = 0; theme < 2; theme++) {
        bsd_feed_set_light(theme ? BSD_LIGHT_DAY : BSD_LIGHT_NIGHT);
        for (size_t i = 0; i < sizeof(stress) / sizeof(stress[0]); i++) {
            char name[64];
            snprintf(name, sizeof(name), "%s_%s", stress[i].name, theme ? "day" : "night");
            bsd_feed_set_link(i == 1 ? BSD_LINK_DISCONNECTED : BSD_LINK_CONNECTED);
            bsd_feed_set_road(stress[i].road, stress[i].kind, stress[i].dir, s_fake_now);
            bsd_feed_set_song(stress[i].song, stress[i].artist, true, s_fake_now);
            bsd_ui_debug_show_car(stress[i].make, stress[i].model);
            shot(dir, name);
        }
    }
    bsd_ui_debug_show_car(NULL, NULL);
    printf("%s (%d spill%s)\n", s_spills ? "LAYOUT SPILLS FOUND" : "no layout spills", s_spills, s_spills == 1 ? "" : "s");

    /* every catalog car, both fonts paths */
    bsd_feed_set_light(BSD_LIGHT_NIGHT);
    int cat_spills = 0;
    for (int i = 0; i < bsd_catalog_count(); i++) {
        bsd_ui_debug_show_car(bsd_catalog_get(i)->make, bsd_catalog_get(i)->name);
        bsd_ui_refresh();
        cat_spills += bsd_ui_debug_check_layout();
    }
    bsd_ui_debug_show_car(NULL, NULL);
    printf("catalog: %d cars checked, %d spills\n", bsd_catalog_count(), cat_spills);
    s_spills += cat_spills;

    /* one frame partway through the text-change animation */
    bsd_ui_set_animations(true);
    bsd_feed_set_road("Interstate 78", BSD_DIR_SIGNED, "East", s_fake_now);
    bsd_feed_set_song("Here Comes the Sun", "The Beatles", true, s_fake_now);
    bsd_ui_refresh(); lv_timer_handler();
    for (int i = 0; i < 4; i++) { s_fake_now += 30; lv_timer_handler(); }   /* ~120 ms of frames */
    lv_draw_buf_t *mid = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
    if (mid) { char p2[512]; snprintf(p2, sizeof(p2), "%s/16_mid_animation.bmp", dir); write_bmp(p2, mid); lv_draw_buf_destroy(mid); }
    s_fake_now += 1000; lv_timer_handler();
    bsd_ui_set_animations(false);

    run_link_tests();
    run_touch_tests();
    run_garage_tests(dir);
    printf("%s\n", s_failures ? "GARAGE/TOUCH TESTS FAILED" : "all garage tests passed");
    print_garage();
    return (s_failures || s_spills) ? 1 : 0;
}

/* ------------------------------------------------------------ main */

int main(int argc, char **argv)
{
    lv_init();

    lv_display_t *disp = lv_sdl_window_create(800, 480);
    lv_sdl_window_set_title(disp, "Backseat Driver - SIMULATOR");
    lv_sdl_mouse_create();
    /* After the window: lv_sdl_window_create() installs SDL_GetTicks as the
     * LVGL tick, which would bypass the fake clock used by --shots. */
    lv_tick_set_cb(sim_clock);

    if (argc >= 3 && strcmp(argv[1], "--shots") == 0) return run_shots(argv[2]);

    bsd_feed_init();
    bsd_game_init(NULL, (uint32_t)time(NULL), sim_clock());
    bsd_ui_create(sim_clock);
    install_dev_keys();
    bsd_demo_start(sim_clock());

    printf("Backseat Driver simulator. Keys: n p d r m f s g x o t a (see sim/main.c)\n");
    fflush(stdout);

    uint32_t last_ui = 0;
    for (;;) {
        uint32_t t = sim_clock();
        bsd_demo_tick(t);
        if (t - last_ui >= 100) { bsd_ui_refresh(); last_ui = t; }
        uint32_t idle = lv_timer_handler();
        usleep((idle > 20 ? 20 : idle) * 1000);
    }
}
