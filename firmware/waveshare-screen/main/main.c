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
#include "coreiot_client.h"
#include "cJSON.h"

static const char *TAG = "collision_dashboard";

static const char *k_sensor_json_keys[SENSOR_MODEL_COUNT] = {
    "front", "rear", "left_front", "left_rear", "right_front", "right_rear",
};

static void on_wifi_status(bool is_connected, const char *ip)
{
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        ui_dashboard_set_iot_status(is_connected, ip);
        esp_lv_adapter_unlock();
    }
}

static void on_mqtt_status(bool is_connected)
{
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        ui_dashboard_set_iot_status(is_connected, NULL);
        esp_lv_adapter_unlock();
    }
}

static void on_mqtt_data(const char *topic, int topic_len, const char *payload, int payload_len)
{
    (void)topic;
    (void)topic_len;

    if (payload_len <= 0 || payload_len >= 512) {
        return;
    }

    char buf[512];
    memcpy(buf, payload, payload_len);
    buf[payload_len] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        return;
    }

    cJSON *values = cJSON_GetObjectItem(json, "values");

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        for (int i = 0; i < SENSOR_MODEL_COUNT; i++) {
            cJSON *item = cJSON_GetObjectItem(json, k_sensor_json_keys[i]);
            if (!item && values) {
                item = cJSON_GetObjectItem(values, k_sensor_json_keys[i]);
            }
            if (item && cJSON_IsNumber(item)) {
                ui_dashboard_update_sensor(i, (uint16_t)item->valuedouble);
            }
        }

        cJSON *relay_item = cJSON_GetObjectItem(json, "relay");
        if (!relay_item && values) {
            relay_item = cJSON_GetObjectItem(values, "relay");
        }
        cJSON *warn_item = cJSON_GetObjectItem(json, "warning_status");
        if (!warn_item && values) {
            warn_item = cJSON_GetObjectItem(values, "warning_status");
        }
        if (relay_item && cJSON_IsString(relay_item)) {
            bool relay_on = strcmp(relay_item->valuestring, "ON") == 0;
            const char *warn_str = (warn_item && cJSON_IsString(warn_item)) ? warn_item->valuestring : NULL;
            ui_dashboard_set_relay_state(relay_on, warn_str);
        }

        esp_lv_adapter_unlock();
    }

    cJSON_Delete(json);
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
        esp_lv_adapter_unlock();
    }

    coreiot_client_set_callbacks(on_wifi_status, on_mqtt_status, on_mqtt_data);
    coreiot_client_init();
}
