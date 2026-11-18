/*
 * Backseat Driver — board entry point.
 *
 * Boots straight into the child screen with the indoor demo running
 * (always labeled DEMO DATA) until the phone app connects over BLE and sends
 * its first message; from then on the screen shows live data.
 */
#include "ble.h"
#include "board.h"
#include "bsd_cars.h"
#include "bsd_demo.h"
#include "bsd_feed.h"
#include "bsd_ui.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "bsd";

static void log_memory(const char *when)
{
    ESP_LOGI(TAG, "%s: internal free %u KB (min %u KB), PSRAM free %u KB",
             when,
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
}

static void ui_task(void *arg)
{
    lv_init();
    if (board_init() != ESP_OK) {
        ESP_LOGE(TAG, "board init failed");
        vTaskDelete(NULL);
        return;
    }

    uint32_t now = board_millis();
    bsd_feed_init();
    bsd_game_init(NULL, esp_random(), now);
    bsd_ui_create(board_millis);
    bsd_demo_start(now);
    bsd_ui_refresh();

    lv_timer_handler();             /* draw the first frame, then light the panel */
    board_backlight(1);
    log_memory("after first frame");
    ble_start();

    uint32_t last_refresh = 0, last_log = board_millis();
    for (;;) {
        uint32_t wait = lv_timer_handler();
        now = board_millis();
        if (ble_poll(now)) bsd_ui_refresh();
        if (now - last_refresh >= 100) {
            last_refresh = now;
            bsd_demo_tick(now);
            bsd_ui_refresh();
        }
        if (now - last_log >= 60000) {
            last_log = now;
            log_memory("running");
        }
        if (wait > 10) wait = 10;
        vTaskDelay(pdMS_TO_TICKS(wait ? wait : 1));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Backseat Driver starting");
    xTaskCreatePinnedToCore(ui_task, "ui", 16 * 1024, NULL, 5, NULL, 1);
}
