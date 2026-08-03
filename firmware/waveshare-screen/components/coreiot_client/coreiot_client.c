/*
 * SPDX-FileCopyrightText: 2026 Vehicle Warning System
 * SPDX-License-Identifier: MIT
 */

#include "coreiot_client.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "coreiot_client";
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_wifi_connected = false;
static int64_t s_last_mqtt_rx_time_ms = 0;

static coreiot_wifi_status_cb_t s_wifi_cb = NULL;
static coreiot_mqtt_status_cb_t s_mqtt_cb = NULL;
static coreiot_data_cb_t s_data_cb = NULL;

void coreiot_client_set_callbacks(coreiot_wifi_status_cb_t wifi_cb,
                                   coreiot_mqtt_status_cb_t mqtt_cb,
                                   coreiot_data_cb_t data_cb)
{
    s_wifi_cb = wifi_cb;
    s_mqtt_cb = mqtt_cb;
    s_data_cb = data_cb;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected to CoreIoT (%s)", COREIOT_MQTT_BROKER_URI);
        if (s_mqtt_cb) {
            s_mqtt_cb(true);
        }

        esp_mqtt_client_subscribe(client, COREIOT_MQTT_TELEMETRY_TOPIC, 1);
        esp_mqtt_client_subscribe(client, "v1/devices/me/attributes", 1);
        esp_mqtt_client_subscribe(client, "v1/devices/me/attributes/response/+", 1);

        char reboot_msg[256];
        snprintf(reboot_msg, sizeof(reboot_msg),
                 "{\"reboot_event\":1,\"device_id\":\"%s\",\"status\":\"ONLINE\",\"reboot_reason\":\"POWER_ON_RESET\",\"wifi_ssid\":\"%s\"}",
                 COREIOT_DEVICE_ID, COREIOT_WIFI_SSID);
        int msg_id = esp_mqtt_client_publish(client, COREIOT_MQTT_TELEMETRY_TOPIC, reboot_msg, 0, 1, 0);
        ESP_LOGI(TAG, "Published reboot telemetry message (msg_id=%d): %s", msg_id, reboot_msg);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT Disconnected");
        if (s_mqtt_cb) {
            s_mqtt_cb(false);
        }
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT DATA received from topic %.*s: %.*s",
                 event->topic_len, event->topic, event->data_len, event->data);
        s_last_mqtt_rx_time_ms = esp_timer_get_time() / 1000;
        if (s_data_cb) {
            s_data_cb(event->topic, event->topic_len, event->data, event->data_len);
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
        ESP_LOGI(TAG, "Wi-Fi STA started, connecting to SSID: %s...", COREIOT_WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *dis_event = (wifi_event_sta_disconnected_t *)event_data;
        s_wifi_connected = false;
        ESP_LOGW(TAG, "Wi-Fi disconnected! Reason code: %d, retrying connection...", dis_event ? dis_event->reason : -1);
        if (s_wifi_cb) {
            s_wifi_cb(false, NULL);
        }
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip_str[32];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Wi-Fi Connected Successfully! IP Address: %s", ip_str);

        s_wifi_connected = true;
        if (s_wifi_cb) {
            s_wifi_cb(true, ip_str);
        }

        if (s_mqtt_client != NULL) {
            esp_mqtt_client_start(s_mqtt_client);
        }
    }
}

void coreiot_client_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = COREIOT_WIFI_SSID,
            .password = COREIOT_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = COREIOT_MQTT_BROKER_URI,
        .credentials.username = COREIOT_MQTT_ACCESS_TOKEN,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
}

int coreiot_client_publish_telemetry(const char *json_payload)
{
    if (s_mqtt_client == NULL || !s_wifi_connected) {
        return -1;
    }
    return esp_mqtt_client_publish(s_mqtt_client, COREIOT_MQTT_TELEMETRY_TOPIC, json_payload, 0, 1, 0);
}

bool coreiot_client_has_recent_data(uint32_t max_age_ms)
{
    if (s_last_mqtt_rx_time_ms == 0) {
        return false;
    }
    int64_t now_ms = esp_timer_get_time() / 1000;
    return (now_ms - s_last_mqtt_rx_time_ms) <= (int64_t)max_age_ms;
}
