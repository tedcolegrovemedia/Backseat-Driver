#include "ble.h"
#include "bsd_demo.h"
#include "bsd_feed.h"
#include "bsd_link.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble";
#define DEVICE_NAME "Backseat Driver"

typedef struct {
    uint16_t len;           /* 0 = link event, see `link` */
    uint8_t  link;          /* bsd_link_t for link events */
    char     data[BSD_LINK_MSG_MAX];
} ble_msg_t;

static QueueHandle_t s_q;
static uint8_t       s_addr_type;

/* AFF99621-C1CF-42E7-B93A-86561359C9B6, little-endian byte order */
static const ble_uuid128_t k_svc = BLE_UUID128_INIT(
    0xb6, 0xc9, 0x59, 0x13, 0x56, 0x86, 0x3a, 0xb9, 0xe7, 0x42, 0xcf, 0xc1, 0x21, 0x96, 0xf9, 0xaf);
/* 472B9109-1271-40ED-A3B9-CD569D58DBA6 */
static const ble_uuid128_t k_feed = BLE_UUID128_INIT(
    0xa6, 0xdb, 0x58, 0x9d, 0x56, 0xcd, 0xb9, 0xa3, 0xed, 0x40, 0x71, 0x12, 0x09, 0x91, 0x2b, 0x47);

static void post_link(bsd_link_t link)
{
    static ble_msg_t m;     /* host task only */
    m.len = 0;
    m.link = (uint8_t)link;
    xQueueSend(s_q, &m, 0);
}

static int feed_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;
    static ble_msg_t m;     /* host task only */
    uint16_t n = OS_MBUF_PKTLEN(ctxt->om);
    if (n == 0 || n >= sizeof(m.data)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if (ble_hs_mbuf_to_flat(ctxt->om, m.data, sizeof(m.data), &n) != 0) return BLE_ATT_ERR_UNLIKELY;
    m.len = n;
    if (xQueueSend(s_q, &m, 0) != pdTRUE) ESP_LOGW(TAG, "queue full, dropped a message");
    return 0;
}

static const struct ble_gatt_svc_def k_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &k_svc.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            { .uuid = &k_feed.u, .access_cb = feed_access,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP },
            { 0 },
        },
    },
    { 0 },
};

static void advertise(void);

static int gap_event(struct ble_gap_event *e, void *arg)
{
    switch (e->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (e->connect.status == 0) {
            ESP_LOGI(TAG, "phone connected");
            post_link(BSD_LINK_CONNECTED);
        } else {
            advertise();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "phone disconnected (reason 0x%x)", e->disconnect.reason);
        post_link(BSD_LINK_DISCONNECTED);
        advertise();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        break;
    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU %d", e->mtu.value);
        break;
    default:
        break;
    }
    return 0;
}

static void advertise(void)
{
    struct ble_hs_adv_fields f = { 0 };
    f.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    f.uuids128 = (ble_uuid128_t *)&k_svc;
    f.num_uuids128 = 1;
    f.uuids128_is_complete = 1;
    ble_gap_adv_set_fields(&f);

    struct ble_hs_adv_fields rsp = { 0 };
    rsp.name = (const uint8_t *)DEVICE_NAME;
    rsp.name_len = strlen(DEVICE_NAME);
    rsp.name_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp);

    struct ble_gap_adv_params p = { .conn_mode = BLE_GAP_CONN_MODE_UND, .disc_mode = BLE_GAP_DISC_MODE_GEN };
    int rc = ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER, &p, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) ESP_LOGE(TAG, "advertise failed: %d", rc);
}

static void on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    ble_hs_id_infer_auto(0, &s_addr_type);
    advertise();
    ESP_LOGI(TAG, "advertising as \"%s\"", DEVICE_NAME);
}

static void host_task(void *arg)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_start(void)
{
    s_q = xQueueCreate(16, sizeof(ble_msg_t));

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    if (nimble_port_init() != ESP_OK) { ESP_LOGE(TAG, "nimble init failed"); return; }
    ble_hs_cfg.sync_cb = on_sync;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(k_services);
    ble_gatts_add_svcs(k_services);
    ble_svc_gap_device_name_set(DEVICE_NAME);
    nimble_port_freertos_init(host_task);
}

bool ble_poll(uint32_t now_ms)
{
    static ble_msg_t m;     /* UI task only */
    bool got = false;
    while (s_q && xQueueReceive(s_q, &m, 0) == pdTRUE) {
        if (m.len == 0) {
            /* The demo shows its own (fake) link state until a phone takes over. */
            if (!bsd_demo_running()) bsd_feed_set_link((bsd_link_t)m.link);
            continue;
        }
        /* The first real message from a phone ends the indoor demo. */
        if (bsd_demo_running()) {
            bsd_demo_stop();
            bsd_feed_clear_road();
            bsd_feed_clear_song();
            ESP_LOGI(TAG, "live data from the phone; demo stopped");
        }
        bsd_feed_set_source(BSD_SRC_LIVE);
        ESP_LOGD(TAG, "msg %.*s", m.len, m.data);
        if (m.data[0] == 'R') ESP_LOGI(TAG, "road: %.*s", m.len, m.data);
        if (!bsd_link_apply(m.data, m.len, now_ms))
            ESP_LOGW(TAG, "bad message (%u bytes, type '%c')", m.len, m.data[0]);
        got = true;
    }
    return got;
}
