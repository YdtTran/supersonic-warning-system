/*
 * SPDX-FileCopyrightText: 2026 Vehicle Warning System
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define COREIOT_WIFI_SSID "HCMUT-MEETING"
#define COREIOT_WIFI_PASS "hcmut@meeting"
#define COREIOT_MQTT_BROKER_URI "mqtt://app.coreiot.io:1883"
#define COREIOT_MQTT_ACCESS_TOKEN "lyeFK1raLOPmjx7bEApw"
#define COREIOT_DEVICE_ID "30287b60-8a67-11f1-84a8-c17e50898235"
#define COREIOT_MQTT_TELEMETRY_TOPIC "v1/devices/me/telemetry"

    /**
     * @brief Callback invoked whenever the Wi-Fi/IP status changes.
     *
     * @param is_connected true if station has an IP address
     * @param ip_str dotted-quad IP string, or NULL when disconnected
     */
    typedef void (*coreiot_wifi_status_cb_t)(bool is_connected, const char *ip_str);

    /**
     * @brief Callback invoked whenever MQTT connection state changes.
     */
    typedef void (*coreiot_mqtt_status_cb_t)(bool is_connected);

    /**
     * @brief Callback invoked whenever a telemetry/attribute JSON payload is received.
     *
     * @param topic MQTT topic string (not null-terminated, use topic_len)
     * @param topic_len length of topic
     * @param payload JSON payload string (not null-terminated, use payload_len)
     * @param payload_len length of payload
     */
    typedef void (*coreiot_data_cb_t)(const char *topic, int topic_len, const char *payload, int payload_len);

    /**
     * @brief Register callbacks before calling coreiot_client_init(). Any may be NULL.
     */
    void coreiot_client_set_callbacks(coreiot_wifi_status_cb_t wifi_cb,
                                      coreiot_mqtt_status_cb_t mqtt_cb,
                                      coreiot_data_cb_t data_cb);

    /**
     * @brief Initializes NVS, Wi-Fi Station connection, and MQTT client for CoreIoT.
     */
    void coreiot_client_init(void);

    /**
     * @brief Publishes a telemetry payload to CoreIoT MQTT server.
     *
     * @param json_payload JSON formatted string
     * @return int msg_id or -1 on error
     */
    int coreiot_client_publish_telemetry(const char *json_payload);

    /**
     * @brief Checks whether live MQTT data was received within max_age_ms milliseconds.
     */
    bool coreiot_client_has_recent_data(uint32_t max_age_ms);

#ifdef __cplusplus
}
#endif
