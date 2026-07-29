/*
 * Sensor Node App Network Header
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_WIFI_SSID            "ACLAB"
#define CONFIG_WIFI_PASS            "ACLAB2023"
#define CONFIG_MQTT_BROKER_URI      "mqtt://app.coreiot.io:1883"
#define CONFIG_MQTT_ACCESS_TOKEN    "OpXQiVBnETXAgVehd2Vg"
#define CONFIG_DEVICE_ID            "d3eb0f80-8a6a-11f1-84a8-c17e50898235"
#define CONFIG_MQTT_TELEMETRY_TOPIC "v1/devices/me/telemetry"

/**
 * @brief Initializes NVS, Wi-Fi Station connection, and MQTT client for CoreIoT.
 */
void app_network_init(void);

/**
 * @brief Returns true if Wi-Fi and MQTT are connected.
 */
bool app_network_is_connected(void);

/**
 * @brief Publishes telemetry payload string to CoreIoT MQTT server.
 * 
 * @param json_payload JSON formatted string
 * @return int msg_id or -1 on error
 */
int app_network_publish_telemetry(const char *json_payload);

/**
 * @brief Returns human readable string for ESP32 reset reason.
 */
const char* get_reset_reason_string(void);

#ifdef __cplusplus
}
#endif
