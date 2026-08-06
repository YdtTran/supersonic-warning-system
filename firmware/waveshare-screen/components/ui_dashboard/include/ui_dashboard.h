/*
 * SPDX-FileCopyrightText: 2026 Vehicle Warning System
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build the full 800x480 collision-avoidance dashboard on the active LVGL screen.
 *        Must be called with the LVGL lock held (e.g. esp_lv_adapter_lock()).
 */
void ui_dashboard_init(void);

/**
 * @brief Push a new distance reading for one of the 6 ultrasonic sensors (S1..S6, id 0..5)
 *        into the dashboard: updates the sidebar reading row, the 2D sensor arc color/state,
 *        and re-evaluates the hazard logic. Must be called with the LVGL lock held.
 *
 * @param sensor_id 0=S1(Front-Left) 1=S2(Front-Right) 2=S3(Mid-Left)
 *                  3=S4(Mid-Right)  4=S5(Rear-Left)    5=S6(Rear-Right)
 * @param dist_cm distance in centimeters
 */
void ui_dashboard_update_sensor(uint8_t sensor_id, uint16_t dist_cm);

/**
 * @brief Update the Wi-Fi/MQTT connectivity badge in the header bar. Must be called with the LVGL lock held.
 *
 * @param is_connected true if MQTT/CoreIoT link is up
 * @param ip station IP string, or NULL if not connected
 */
void ui_dashboard_set_iot_status(bool is_connected, const char *ip);

/**
 * @brief Force-set the "CROSSING TRAFFIC HAZARD" pedestrian-crossing banner independent of the
 *        automatic hazard evaluator. Must be called with the LVGL lock held.
 */
void ui_dashboard_set_hazard_warning(bool is_pedestrian_crossing_risk);

/**
 * @brief Display the relay state and warning status as computed server-side by the CoreIoT
 *        rule chain (fields "relay" and "warning_status" from the telemetry/attribute JSON).
 *        This is independent of the locally-evaluated "OVERALL" hazard label. Must be called
 *        with the LVGL lock held.
 *
 * @param relay_on true if the rule chain reports relay == "ON"
 * @param warning_status raw status string from the server (e.g. "NORMAL", "WARNING", "DANGER"), or NULL
 */
void ui_dashboard_set_relay_state(bool relay_on, const char *warning_status);

/**
 * @brief Display the sensor-node's physical buzzer state (field "buzzer" from the telemetry/
 *        attribute JSON, computed alongside "relay" by the CoreIoT rule chain). The buzzer
 *        itself is driven locally on the sensor-node from live distance readings; this only
 *        mirrors that state on the dashboard. Must be called with the LVGL lock held.
 *
 * @param buzzer_on true if the sensor-node reports buzzer == "ON"
 */
void ui_dashboard_set_buzzer_state(bool buzzer_on);

/**
 * @brief Update the ESP-NOW link badge in the top-right of the header bar. Used instead of
 *        ui_dashboard_set_iot_status() when sensor-node talks directly to waveshare-screen over
 *        ESP-NOW rather than via MQTT/CoreIoT (see docs/architecture/ESPNOW_NETWORK.md). Must be
 *        called with the LVGL lock held.
 *
 * @param linked true if a sensor-node ESP-NOW packet has been received within the watchdog window
 */
void ui_dashboard_set_espnow_status(bool linked);

#ifdef __cplusplus
}
#endif
