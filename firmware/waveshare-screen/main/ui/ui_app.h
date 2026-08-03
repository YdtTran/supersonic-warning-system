/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the Control & Monitoring Dashboard UI canvas (800x480).
 */
void ui_app_init(void);

/**
 * @brief Dynamically updates a row in the telemetry table and triggers a 300ms soft glow flash animation.
 * 
 * @param row_idx Row index (0: Temp, 1: Humidity, 2: Distance, 3: Relay)
 * @param val Value string (e.g. "28.5")
 * @param updated Last updated string (e.g. "Just now")
 */
void ui_app_update_telemetry(int row_idx, const char *val, const char *updated);

/**
 * @brief Dynamically updates the MQTT connection status badge in the header.
 * 
 * @param connected True if connected, false otherwise
 * @param broker Broker hostname (e.g. "broker.coreiot.io")
 */
void ui_app_update_mqtt_status(bool connected, const char *broker);

/**
 * @brief Adds a new sample point to the 10-point mini sparkline chart.
 * 
 * @param val Value sample (e.g., distance in cm)
 */
void ui_app_add_sparkline_point(int32_t val);

/**
 * @brief Dynamically updates the Wi-Fi status label in the header.
 * 
 * @param ssid SSID name
 * @param ip IP address string
 */
void ui_app_update_wifi_info(const char *ssid, const char *ip);

/**
 * @brief Updates the Warning Status Badge on the header/dashboard based on CoreIoT Rule-Chain output.
 * 
 * @param status Warning status string ("SAFE", "NORMAL", "APPROACHING", "WARNING", "DANGER", "CRITICAL")
 * @param distance Current distance reading in cm (-1.0f if unknown)
 * @param vehicle_detected True if vehicle detected, false otherwise
 */
void ui_app_update_warning_status(const char *status, float distance, bool vehicle_detected);

#ifdef __cplusplus
}
#endif


