/*
 * Sensor Node App Network Implementation
 */

#include "app_network.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "esp_system.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "sensor_net";
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_wifi_connected = false;
static bool s_mqtt_connected = false;

const char* get_reset_reason_string(void)
{
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON:   return "POWERON_RESET";
        case ESP_RST_EXT:       return "EXTERNAL_PIN_RESET";
        case ESP_RST_SW:        return "SW_RESET";
        case ESP_RST_PANIC:     return "PANIC_RESET";
        case ESP_RST_INT_WDT:   return "INT_WDT_RESET";
        case ESP_RST_TASK_WDT:  return "TASK_WDT_RESET";
        case ESP_RST_WDT:       return "WDT_RESET";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP_RESET";
        case ESP_RST_BROWNOUT:  return "BROWNOUT_RESET";
        case ESP_RST_SDIO:      return "SDIO_RESET";
        default:                return "POWERON_RESET";
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT Connected to CoreIoT (%s)", CONFIG_MQTT_BROKER_URI);

        // Send boot event payload upon connection
        char reboot_msg[256];
        snprintf(reboot_msg, sizeof(reboot_msg),
                 "{\"device-id\":\"%s\",\"device_id\":\"%s\",\"reboot-reason\":\"%s\",\"reboot_reason\":\"%s\",\"status\":\"ONLINE\"}",
                 CONFIG_DEVICE_ID, CONFIG_DEVICE_ID, get_reset_reason_string(), get_reset_reason_string());

        int msg_id = esp_mqtt_client_publish(client, CONFIG_MQTT_TELEMETRY_TOPIC, reboot_msg, 0, 1, 0);
        ESP_LOGI(TAG, "Published initial boot/reboot telemetry message (msg_id=%d): %s", msg_id, reboot_msg);
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT Disconnected from CoreIoT");
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
        s_mqtt_connected = false;
        ESP_LOGW(TAG, "Wi-Fi disconnected! Reason code: %d, retrying connection...", dis_event ? dis_event->reason : -1);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip_str[32];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Wi-Fi Connected Successfully! IP Address: %s", ip_str);

        s_wifi_connected = true;

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

bool app_network_is_connected(void)
{
    return s_wifi_connected && s_mqtt_connected;
}

int app_network_publish_telemetry(const char *json_payload)
{
    if (s_mqtt_client == NULL || !s_wifi_connected || !s_mqtt_connected) {
        return -1;
    }
    return esp_mqtt_client_publish(s_mqtt_client, CONFIG_MQTT_TELEMETRY_TOPIC, json_payload, 0, 1, 0);
}
