/*
 * board.c — direct glue between LVGL 9.5 and the Waveshare 4.3" board.
 *
 * Pins and panel timing: Waveshare docs page (pins) and their ESP-IDF demo
 * (16 MHz PCLK, 4/8/8 porches). The CH422G is driven with raw I2C writes:
 * it answers on fixed addresses that act as registers.
 */
#include "board.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "board";

/* ---- pins ---- */
#define PIN_I2C_SDA   8
#define PIN_I2C_SCL   9
#define PIN_TOUCH_INT 4
#define PIN_PCLK      7
#define PIN_DE        5
#define PIN_HSYNC     46
#define PIN_VSYNC     3
static const int k_data_pins[16] = {
    14, 38, 18, 17, 10,         /* B3..B7 */
    39, 0, 45, 48, 47, 21,      /* G2..G7 */
    1, 2, 42, 41, 40,           /* R3..R7 */
};

/* ---- CH422G ---- */
#define CH422G_ADDR_SET 0x24    /* system register: 0x01 = IO0-7 push-pull outputs */
#define CH422G_ADDR_OUT 0x38    /* IO0-7 output levels                           */
#define EXIO_TP_RST  (1 << 1)
#define EXIO_BL      (1 << 2)
#define EXIO_LCD_RST (1 << 3)
#define EXIO_SD_CS   (1 << 4)   /* active low: keep high = card deselected       */
#define EXIO_USB_SEL (1 << 5)   /* matches Waveshare's demo value                */

#define LVGL_BUF_LINES 40

static i2c_master_bus_handle_t s_i2c;
static i2c_master_dev_handle_t s_ch_set, s_ch_out;
static uint8_t                 s_exio;
static esp_lcd_panel_handle_t  s_panel;
static esp_lcd_touch_handle_t  s_touch;

uint32_t board_millis(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static esp_err_t exio_write(uint8_t v)
{
    s_exio = v;
    return i2c_master_transmit(s_ch_out, &v, 1, 100);
}

esp_err_t board_backlight(int on)
{
    return exio_write(on ? (s_exio | EXIO_BL) : (s_exio & ~EXIO_BL));
}

static esp_err_t i2c_and_expander_init(void)
{
    i2c_master_bus_config_t bus = {
        .i2c_port = 0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus, &s_i2c), TAG, "i2c bus");

    i2c_device_config_t dev = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .scl_speed_hz = 400000 };
    dev.device_address = CH422G_ADDR_SET;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c, &dev, &s_ch_set), TAG, "ch422g set");
    dev.device_address = CH422G_ADDR_OUT;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c, &dev, &s_ch_out), TAG, "ch422g out");

    uint8_t mode = 0x01;
    ESP_RETURN_ON_ERROR(i2c_master_transmit(s_ch_set, &mode, 1, 100), TAG, "ch422g mode");

    /* GT911 address select: hold INT low while releasing reset -> 0x5D.
     * Backlight stays off until the first frame is drawn. */
    ESP_RETURN_ON_ERROR(exio_write(EXIO_LCD_RST | EXIO_SD_CS | EXIO_USB_SEL), TAG, "exio");
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_config_t io = { .pin_bit_mask = 1ULL << PIN_TOUCH_INT, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io);
    gpio_set_level(PIN_TOUCH_INT, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(exio_write(s_exio | EXIO_TP_RST), TAG, "touch reset");
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_direction(PIN_TOUCH_INT, GPIO_MODE_INPUT);
    return ESP_OK;
}

static esp_err_t panel_init(void)
{
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = 16 * 1000 * 1000,
            .h_res = BOARD_H_RES,
            .v_res = BOARD_V_RES,
            .hsync_pulse_width = 4, .hsync_back_porch = 8, .hsync_front_porch = 8,
            .vsync_pulse_width = 4, .vsync_back_porch = 8, .vsync_front_porch = 8,
            .flags.pclk_active_neg = 1,
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 1,
        .bounce_buffer_size_px = BOARD_H_RES * 10,   /* keeps the panel fed from PSRAM */
        .hsync_gpio_num = PIN_HSYNC,
        .vsync_gpio_num = PIN_VSYNC,
        .de_gpio_num = PIN_DE,
        .pclk_gpio_num = PIN_PCLK,
        .disp_gpio_num = -1,
        .flags.fb_in_psram = 1,
    };
    for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = k_data_pins[i];
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&cfg, &s_panel), TAG, "rgb panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init");
    return ESP_OK;
}

static esp_err_t touch_init(void)
{
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.scl_speed_hz = 400000;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c, &io_cfg, &io), TAG, "touch io");
    esp_lcd_touch_config_t tp = {
        .x_max = BOARD_H_RES,
        .y_max = BOARD_V_RES,
        .rst_gpio_num = -1,     /* reset is on the expander, done above */
        .int_gpio_num = -1,     /* polled from the LVGL task             */
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(io, &tp, &s_touch), TAG, "gt911");
    return ESP_OK;
}

/* ---- LVGL glue ---- */
static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    /* The RGB driver copies into its PSRAM frame buffer before returning. */
    esp_lcd_panel_draw_bitmap(s_panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, px);
    lv_display_flush_ready(d);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t x, y, strength;
    uint8_t  n = 0;
    esp_lcd_touch_read_data(s_touch);
    if (esp_lcd_touch_get_coordinates(s_touch, &x, &y, &strength, &n, 1) && n > 0) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void *alloc_draw_buf(size_t bytes)
{
    void *p = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!p) {
        ESP_LOGW(TAG, "draw buffer falls back to PSRAM");
        p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    }
    return p;
}

esp_err_t board_init(void)
{
    ESP_RETURN_ON_ERROR(i2c_and_expander_init(), TAG, "expander");
    ESP_RETURN_ON_ERROR(panel_init(), TAG, "panel");
    esp_err_t t = touch_init();
    if (t != ESP_OK) ESP_LOGE(TAG, "touch not available (%s); display only", esp_err_to_name(t));

    lv_tick_set_cb(board_millis);
    lv_display_t *disp = lv_display_create(BOARD_H_RES, BOARD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    size_t bytes = BOARD_H_RES * LVGL_BUF_LINES * sizeof(uint16_t);
    void *b1 = alloc_draw_buf(bytes), *b2 = alloc_draw_buf(bytes);
    if (!b1 || !b2) return ESP_ERR_NO_MEM;
    lv_display_set_buffers(disp, b1, b2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);

    if (s_touch) {
        lv_indev_t *in = lv_indev_create();
        lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(in, touch_read_cb);
        lv_indev_set_display(in, disp);
    }
    return ESP_OK;
}
