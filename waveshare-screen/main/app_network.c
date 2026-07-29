/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include "app_network.h"
#include "ui_app.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "esp_lv_adapter.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "app_network";
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_wifi_connected = false;
static int64_t s_last_mqtt_rx_time_ms = 0;

static cJSON *get_json_field(cJSON *root, cJSON *values_obj, const char *field_name)
{
    cJSON *item = cJSON_GetObjectItem(root, field_name);
    if (!item && values_obj && cJSON_IsObject(values_obj)) {
        item = cJSON_GetObjectItem(values_obj, field_name);
    }
    return item;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected to CoreIoT (%s)", CONFIG_MQTT_BROKER_URI);
        if (esp_lv_adapter_lock(-1) == ESP_OK) {
            ui_app_update_mqtt_status(true, "app.coreiot.io");
            esp_lv_adapter_unlock();
        }

        // Subscribe to CoreIoT telemetry & attributes topics
        esp_mqtt_client_subscribe(client, CONFIG_MQTT_TELEMETRY_TOPIC, 1);
        esp_mqtt_client_subscribe(client, "v1/devices/me/attributes", 1);
        esp_mqtt_client_subscribe(client, "v1/devices/me/attributes/response/+", 1);

        // Fallback: Publish Reboot Message for CoreIoT Timeseries storage
        char reboot_msg[256];
        snprintf(reboot_msg, sizeof(reboot_msg),
                 "{\"reboot_event\":1,\"device_id\":\"%s\",\"status\":\"ONLINE\",\"reboot_reason\":\"POWER_ON_RESET\",\"wifi_ssid\":\"%s\"}",
                 CONFIG_DEVICE_ID, CONFIG_WIFI_SSID);

        int msg_id = esp_mqtt_client_publish(client, CONFIG_MQTT_TELEMETRY_TOPIC, reboot_msg, 0, 1, 0);
        ESP_LOGI(TAG, "Published reboot telemetry message to timeseries (msg_id=%d): %s", msg_id, reboot_msg);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT Disconnected");
        if (esp_lv_adapter_lock(-1) == ESP_OK) {
            ui_app_update_mqtt_status(false, "app.coreiot.io");
            esp_lv_adapter_unlock();
        }
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT DATA received from topic %.*s: %.*s",
                 event->topic_len, event->topic, event->data_len, event->data);

        // Parse JSON telemetry payload if available
        if (event->data_len > 0) {
            char buf[512];
            int len = event->data_len < (int)sizeof(buf) - 1 ? event->data_len : (int)sizeof(buf) - 1;
            memcpy(buf, event->data, len);
            buf[len] = '\0';

            cJSON *json = cJSON_Parse(buf);
            if (json != NULL) {
                s_last_mqtt_rx_time_ms = esp_timer_get_time() / 1000;

                cJSON *values_obj = cJSON_GetObjectItem(json, "values");

                cJSON *item_dist = get_json_field(json, values_obj, "distance");
                if (!item_dist) item_dist = get_json_field(json, values_obj, "distance_cm");

                cJSON *item_temp = get_json_field(json, values_obj, "temperature");
                if (!item_temp) item_temp = get_json_field(json, values_obj, "temp");

                cJSON *item_hum = get_json_field(json, values_obj, "humidity");
                if (!item_hum) item_hum = get_json_field(json, values_obj, "hum");

                cJSON *item_relay = get_json_field(json, values_obj, "relay");
                if (!item_relay) item_relay = get_json_field(json, values_obj, "relay_status");

                cJSON *item_warn = get_json_field(json, values_obj, "warning_status");
                if (!item_warn) item_warn = get_json_field(json, values_obj, "status");

                cJSON *item_veh = get_json_field(json, values_obj, "vehicle_detected");
                if (!item_veh) item_veh = get_json_field(json, values_obj, "vehicle");

                float dist_val = -1.0f;
                bool vehicle_detected = false;
                const char *warn_str = NULL;

                if (item_dist != NULL) {
                    if (cJSON_IsNumber(item_dist)) {
                        dist_val = (float)item_dist->valuedouble;
                    } else if (cJSON_IsString(item_dist) && item_dist->valuestring != NULL) {
                        dist_val = strtof(item_dist->valuestring, NULL);
                    }
                }

                if (item_veh != NULL) {
                    if (cJSON_IsBool(item_veh)) {
                        vehicle_detected = cJSON_IsTrue(item_veh);
                    } else if (cJSON_IsNumber(item_veh)) {
                        vehicle_detected = (item_veh->valueint != 0);
                    } else if (cJSON_IsString(item_veh) && item_veh->valuestring != NULL) {
                        vehicle_detected = (strcasecmp(item_veh->valuestring, "true") == 0 || strcmp(item_veh->valuestring, "1") == 0);
                    }
                } else if (dist_val >= 0.0f) {
                    vehicle_detected = (dist_val < 50.0f);
                }

                if (item_warn != NULL && cJSON_IsString(item_warn) && item_warn->valuestring != NULL) {
                    warn_str = item_warn->valuestring;
                }

                if (esp_lv_adapter_lock(-1) == ESP_OK) {
                    if (dist_val >= 0.0f) {
                        char str_val[16];
                        snprintf(str_val, sizeof(str_val), "%.1f", dist_val);
                        ui_app_update_telemetry(2, str_val, "Just now");
                        ui_app_add_sparkline_point((int32_t)dist_val);
                    }

                    if (item_temp && cJSON_IsNumber(item_temp)) {
                        char str_val[16];
                        snprintf(str_val, sizeof(str_val), "%.1f", item_temp->valuedouble);
                        ui_app_update_telemetry(0, str_val, "Just now");
                    }

                    if (item_hum && cJSON_IsNumber(item_hum)) {
                        char str_val[16];
                        snprintf(str_val, sizeof(str_val), "%.1f", item_hum->valuedouble);
                        ui_app_update_telemetry(1, str_val, "Just now");
                    }

                    if (item_relay) {
                        char str_val[16];
                        if (cJSON_IsString(item_relay) && item_relay->valuestring) {
                            snprintf(str_val, sizeof(str_val), "%s", item_relay->valuestring);
                        } else if (cJSON_IsNumber(item_relay)) {
                            snprintf(str_val, sizeof(str_val), "%s", item_relay->valueint ? "ON" : "OFF");
                        } else if (cJSON_IsBool(item_relay)) {
                            snprintf(str_val, sizeof(str_val), "%s", cJSON_IsTrue(item_relay) ? "ON" : "OFF");
                        } else {
                            snprintf(str_val, sizeof(str_val), "%s", vehicle_detected ? "ON" : "OFF");
                        }
                        ui_app_update_telemetry(3, str_val, "Just now");
                    }

                    ui_app_update_warning_status(warn_str, dist_val, vehicle_detected);
                    esp_lv_adapter_unlock();
                }

                cJSON_Delete(json);
            }
        }
        break;

    default:
        break;
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA started, connecting to SSID: %s...", CONFIG_WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *dis_event = (wifi_event_sta_disconnected_t *)event_data;
        s_wifi_connected = false;
        ESP_LOGW(TAG, "Wi-Fi disconnected! Reason code: %d, retrying connection...", dis_event ? dis_event->reason : -1);
        if (esp_lv_adapter_lock(-1) == ESP_OK) {
            ui_app_update_wifi_info(CONFIG_WIFI_SSID, NULL);
            esp_lv_adapter_unlock();
        }
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip_str[32];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Wi-Fi Connected Successfully! IP Address: %s", ip_str);

        s_wifi_connected = true;
        if (esp_lv_adapter_lock(-1) == ESP_OK) {
            ui_app_update_wifi_info(CONFIG_WIFI_SSID, ip_str);
            esp_lv_adapter_unlock();
        }

        // Start MQTT client if not started
        if (s_mqtt_client != NULL) {
            esp_mqtt_client_start(s_mqtt_client);
        }
    }
}

void app_network_init(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Network Interface & Event Loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
        .credentials.username = CONFIG_MQTT_ACCESS_TOKEN,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
}

int app_network_publish_telemetry(const char *json_payload)
{
    if (s_mqtt_client == NULL || !s_wifi_connected) {
        return -1;
    }
    return esp_mqtt_client_publish(s_mqtt_client, CONFIG_MQTT_TELEMETRY_TOPIC, json_payload, 0, 1, 0);
}

bool app_network_has_recent_data(uint32_t max_age_ms)
{
    if (s_last_mqtt_rx_time_ms == 0) return false;
    int64_t now_ms = esp_timer_get_time() / 1000;
    return (now_ms - s_last_mqtt_rx_time_ms) <= (int64_t)max_age_ms;
}
