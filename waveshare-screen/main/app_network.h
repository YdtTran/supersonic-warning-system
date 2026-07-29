/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
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
#define CONFIG_MQTT_ACCESS_TOKEN    "lyeFK1raLOPmjx7bEApw"
#define CONFIG_DEVICE_ID            "30287b60-8a67-11f1-84a8-c17e50898235"
#define CONFIG_MQTT_TELEMETRY_TOPIC "v1/devices/me/telemetry"

/**
 * @brief Initializes NVS, Wi-Fi Station connection, and MQTT client for CoreIoT.
 */
void app_network_init(void);

/**
 * @brief Publishes a telemetry payload to CoreIoT MQTT server.
 * 
 * @param json_payload JSON formatted string
 * @return int msg_id or -1 on error
 */
int app_network_publish_telemetry(const char *json_payload);

/**
 * @brief Checks whether live MQTT data was received within max_age_ms milliseconds.
 * 
 * @param max_age_ms Maximum age in milliseconds
 * @return true if recent live data exists, false otherwise
 */
bool app_network_has_recent_data(uint32_t max_age_ms);

#ifdef __cplusplus
}
#endif
