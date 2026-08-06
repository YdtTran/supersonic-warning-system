/*
 * SPDX-FileCopyrightText: 2026 Vehicle Warning System
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "waveshare_rgb_lcd_port.h"
#include "sensor_model.h"
#include "ui_dashboard.h"

#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "collision_dashboard";

// Callers here run on the Wi-Fi event loop task / esp-now recv task, not the LVGL
// task, so a bounded timeout is used instead of an infinite wait: if the LVGL
// task is ever stalled, this must not block network event processing forever.
#define LV_LOCK_TIMEOUT_TICKS pdMS_TO_TICKS(100)

// =========================================================
// ESP-NOW wire schema - direct link from firmware/sensor-node, replacing the
// MQTT/CoreIoT path in this branch (see docs/architecture/ESPNOW_NETWORK.md).
// No shared header between the two PlatformIO projects, so this struct and
// the channel below must be kept manually in sync with
// firmware/sensor-node/include/EspNowConfig.h.
// =========================================================

#define ESPNOW_CHANNEL 1
#define ESPNOW_SENSOR_SLOT_COUNT 6
#define ESPNOW_LINK_TIMEOUT_MS 1500

typedef struct __attribute__((packed)) {
    float distance_cm[ESPNOW_SENSOR_SLOT_COUNT];
    uint8_t valid[ESPNOW_SENSOR_SLOT_COUNT];
} espnow_sensor_msg_t;

static volatile bool s_espnow_linked = false;
static volatile int64_t s_last_recv_us = 0;

static void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    (void)info;

    if (len != (int)sizeof(espnow_sensor_msg_t)) {
        ESP_LOGW(TAG, "Dropped ESP-NOW packet with unexpected size %d", len);
        return;
    }

    espnow_sensor_msg_t msg;
    memcpy(&msg, data, sizeof(msg));
    s_last_recv_us = esp_timer_get_time();

    if (esp_lv_adapter_lock(LV_LOCK_TIMEOUT_TICKS) == ESP_OK) {
        if (!s_espnow_linked) {
            s_espnow_linked = true;
            ui_dashboard_set_espnow_status(true);
        }

        for (int i = 0; i < ESPNOW_SENSOR_SLOT_COUNT; i++) {
            if (msg.valid[i]) {
                ui_dashboard_update_sensor((uint8_t)i, (uint16_t)msg.distance_cm[i]);
            } else {
                // valid=0 is sensor-node's explicit "null" for this slot (no hardware
                // wired, or the reading was dropped) - clear it instead of leaving the
                // last stale distance on screen.
                ui_dashboard_clear_sensor((uint8_t)i);
            }
        }

        esp_lv_adapter_unlock();
    }
}

// Periodic watchdog (esp_timer, 1s tick): if no ESP-NOW packet has arrived within
// ESPNOW_LINK_TIMEOUT_MS, drop the header badge to "NO LINK" - sensor-node sends every 500ms,
// so a real link loss is caught within 1.5-2s.
static void espnow_watchdog_cb(void *arg)
{
    (void)arg;

    if (!s_espnow_linked) {
        return;
    }

    int64_t elapsed_ms = (esp_timer_get_time() - s_last_recv_us) / 1000;
    if (elapsed_ms <= ESPNOW_LINK_TIMEOUT_MS) {
        return;
    }

    if (esp_lv_adapter_lock(LV_LOCK_TIMEOUT_TICKS) == ESP_OK) {
        s_espnow_linked = false;
        ui_dashboard_set_espnow_status(false);
        esp_lv_adapter_unlock();
    }
}

// =========================================================
// NETWORK TASK
// Khởi tạo ESP-NOW receiver (Wi-Fi STA ở chế độ trần, không kết nối AP nào,
// chỉ cố định channel để nhận gói ESP-NOW từ sensor-node) trong một FreeRTOS
// task riêng, tách khỏi task LVGL - cùng phong cách "networkTask" tách biệt
// như firmware/sensor-node.
// =========================================================

static void networkTask(void *pvParameters)
{
    (void)pvParameters;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // esp_netif_create_default_wifi_sta() returns esp_netif_t*, not esp_err_t - it cannot be
    // wrapped in ESP_ERROR_CHECK.
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif != NULL);

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "Own MAC: " MACSTR " (update ESPNOW_PEER_MAC in sensor-node if this changed)", MAC2STR(mac));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));
    ESP_LOGI(TAG, "ESP-NOW receiver ready on channel %d", ESPNOW_CHANNEL);

    const esp_timer_create_args_t watchdog_args = {
        .callback = espnow_watchdog_cb,
        .name = "espnow_wdt",
    };
    esp_timer_handle_t watchdog_timer;
    ESP_ERROR_CHECK(esp_timer_create(&watchdog_args, &watchdog_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(watchdog_timer, 1000000)); // 1s

    vTaskDelete(NULL);
}

void app_main(void)
{
    const esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;
    const esp_lv_adapter_tear_avoid_mode_t tear_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_touch_handle_t touch_handle = NULL;

    ESP_ERROR_CHECK(waveshare_esp32_s3_rgb_lcd_init(
        tear_mode,
        rotation,
        &panel_handle,
        &touch_handle));
    ESP_ERROR_CHECK(waveshare_rgb_lcd_backlight_on());

    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_config.task_stack_size = 12 * 1024;
    adapter_config.stack_in_psram = true;
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    esp_lv_adapter_display_config_t disp_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
        panel_handle,
        NULL,
        EXAMPLE_LCD_H_RES,
        EXAMPLE_LCD_V_RES,
        rotation);
    disp_config.profile.use_psram = true;

    lv_display_t *disp = esp_lv_adapter_register_display(&disp_config);
    assert(disp != NULL);

    if (touch_handle != NULL) {
        esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
        lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_config);
        assert(touch != NULL);
    }

    ESP_ERROR_CHECK(esp_lv_adapter_start());

    ESP_LOGI(TAG, "Initializing Collision-Avoidance Dashboard");
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        ui_dashboard_init();
        ui_dashboard_set_espnow_status(false);
        ui_dashboard_set_relay_state(false, "N/A (ESP-NOW mode)");
        esp_lv_adapter_unlock();
    }

    xTaskCreatePinnedToCore(
        networkTask,
        "NetworkTask",
        4096,
        NULL,
        1,
        NULL,
        0);
}
