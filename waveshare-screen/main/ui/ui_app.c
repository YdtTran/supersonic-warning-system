/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include "ui_app.h"
#include "ui_anim.h"
#include "app_network.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui_app";

typedef struct {
    lv_obj_t *row_container;
    lv_obj_t *lbl_param;
    lv_obj_t *lbl_val;
    lv_obj_t *lbl_unit;
    lv_obj_t *lbl_updated;
    uint32_t bg_hex;
} telemetry_row_t;

static telemetry_row_t s_rows[4];
static lv_obj_t *s_lbl_mqtt_status = NULL;
static lv_obj_t *s_lbl_wifi = NULL;
static lv_obj_t *s_lbl_key_val = NULL;
static bool s_key_visible = false;
static lv_obj_t *s_chart = NULL;
static lv_chart_series_t *s_chart_series = NULL;
static lv_obj_t *s_modal = NULL;
static lv_obj_t *s_modal_ta = NULL;
static lv_obj_t *s_warning_badge = NULL;
static lv_obj_t *s_lbl_warning_status = NULL;

static void show_hide_key_cb(lv_event_t *e)
{
    (void)e;
    s_key_visible = !s_key_visible;
    if (s_lbl_key_val != NULL) {
        if (s_key_visible) {
            lv_label_set_text(s_lbl_key_val, "lyeFK1raLOPmjx7bEApw");
        } else {
            lv_label_set_text(s_lbl_key_val, "••••••••••••3A8F");
        }
    }
}

static void close_settings_modal_cb(lv_event_t *e)
{
    (void)e;
    if (s_modal != NULL) {
        lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    }
}

static void open_settings_modal_cb(lv_event_t *e)
{
    (void)e;
    if (s_modal != NULL) {
        lv_obj_remove_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_t *scr = lv_screen_active();

    // Create modal dialog surface (Dark Slate Gray container with Bright Blue accent border)
    s_modal = lv_obj_create(scr);
    lv_obj_set_size(s_modal, 720, 420);
    lv_obj_align(s_modal, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_modal, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_bg_opa(s_modal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_modal, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_border_width(s_modal, 2, 0);
    lv_obj_set_style_radius(s_modal, 12, 0);
    lv_obj_set_style_pad_all(s_modal, 12, 0);

    lv_obj_t *title = lv_label_create(s_modal);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " Wi-Fi & MQTT Key Settings");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 5);

    // Close / Save Button
    lv_obj_t *btn_close = lv_button_create(s_modal);
    lv_obj_set_size(btn_close, 140, 48);
    lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, -10, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_radius(btn_close, 8, 0);
    lv_obj_add_event_cb(btn_close, close_settings_modal_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, "Save & Close");
    lv_obj_set_style_text_color(lbl_close, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_close);

    // Text Area for editing Access Key
    s_modal_ta = lv_textarea_create(s_modal);
    lv_obj_set_size(s_modal_ta, 520, 48);
    lv_obj_align(s_modal_ta, LV_ALIGN_TOP_LEFT, 10, 40);
    lv_textarea_set_placeholder_text(s_modal_ta, "Enter MQTT Access Token...");
    lv_textarea_set_text(s_modal_ta, "lyeFK1raLOPmjx7bEApw");
    lv_obj_set_style_bg_color(s_modal_ta, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_text_color(s_modal_ta, lv_color_hex(0x3B82F6), 0);

    // Virtual Keyboard
    lv_obj_t *kb = lv_keyboard_create(s_modal);
    lv_obj_set_size(kb, 680, 280);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, s_modal_ta);
}

static void publish_test_cb(lv_event_t *e)
{
    (void)e;
    // Send a telemetry ping request to CoreIoT MQTT server (Server is root of trust)
    const char *ping_payload = "{\"ping\":true,\"client\":\"waveshare_screen\",\"request\":\"get_telemetry\"}";
    app_network_publish_telemetry(ping_payload);

    ESP_LOGI(TAG, "Sent Telemetry Request Ping to CoreIoT server: %s", ping_payload);
}


static void clear_logs_cb(lv_event_t *e)
{
    (void)e;
    for (int i = 0; i < 4; i++) {
        ui_app_update_telemetry(i, "--", "Cleared");
    }
    ESP_LOGI(TAG, "Telemetry logs cleared");
}

static void reconnect_cb(lv_event_t *e)
{
    (void)e;
    ui_app_update_mqtt_status(true, "broker.coreiot.io");
    ESP_LOGI(TAG, "Reconnected to CoreIoT broker");
}

void ui_app_init(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* ===================================================================
     * 1. HEADER BAR (800 x 50 px)
     * =================================================================== */
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, 800, 50);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 8, 0);

    // Left: System Title
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "CoreIoT Monitor v1.0");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    // Center: MQTT Connection Badge
    s_lbl_mqtt_status = lv_label_create(header);
    lv_label_set_text(s_lbl_mqtt_status, LV_SYMBOL_BULLET " MQTT: CONNECTED at broker.coreiot.io");
    lv_obj_set_style_text_color(s_lbl_mqtt_status, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_text_font(s_lbl_mqtt_status, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_mqtt_status, LV_ALIGN_CENTER, -20, 0);

    // Right: Wi-Fi & IP Info
    s_lbl_wifi = lv_label_create(header);
    lv_label_set_text(s_lbl_wifi, LV_SYMBOL_WIFI " Wi-Fi: Connecting...");
    lv_obj_set_style_text_color(s_lbl_wifi, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(s_lbl_wifi, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_wifi, LV_ALIGN_RIGHT_MID, -10, 0);


    /* ===================================================================
     * 2. LEFT PANEL - CONFIG & SECURITY (260 x 415 px)
     * =================================================================== */
    lv_obj_t *left_panel = lv_obj_create(scr);
    lv_obj_set_size(left_panel, 260, 415);
    lv_obj_align(left_panel, LV_ALIGN_TOP_LEFT, 10, 58);
    lv_obj_set_style_bg_color(left_panel, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_bg_opa(left_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(left_panel, 10, 0);
    lv_obj_set_style_border_width(left_panel, 0, 0);
    lv_obj_set_style_pad_all(left_panel, 12, 0);

    // Server Card
    lv_obj_t *server_title = lv_label_create(left_panel);
    lv_label_set_text(server_title, LV_SYMBOL_DIRECTORY " SERVER CONFIG");
    lv_obj_set_style_text_color(server_title, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_text_font(server_title, &lv_font_montserrat_14, 0);
    lv_obj_align(server_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *server_info = lv_label_create(left_panel);
    lv_label_set_text(server_info, "Host: app.coreiot.io\nPort: 1883 / 8883\nKeepalive: 60s");
    lv_obj_set_style_text_color(server_info, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_text_font(server_info, &lv_font_montserrat_14, 0);
    lv_obj_align(server_info, LV_ALIGN_TOP_LEFT, 0, 25);

    // Credentials Card
    lv_obj_t *cred_title = lv_label_create(left_panel);
    lv_label_set_text(cred_title, LV_SYMBOL_FILE " CREDENTIALS");
    lv_obj_set_style_text_color(cred_title, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_text_font(cred_title, &lv_font_montserrat_14, 0);
    lv_obj_align(cred_title, LV_ALIGN_TOP_LEFT, 0, 105);

    lv_obj_t *dev_id_info = lv_label_create(left_panel);
    lv_label_set_text(dev_id_info, "Device ID: SENSOR-ESP32S3\nAccess Token:");
    lv_obj_set_style_text_color(dev_id_info, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_text_font(dev_id_info, &lv_font_montserrat_14, 0);
    lv_obj_align(dev_id_info, LV_ALIGN_TOP_LEFT, 0, 130);

    s_lbl_key_val = lv_label_create(left_panel);
    lv_label_set_text(s_lbl_key_val, "••••••••••••3A8F");
    lv_obj_set_style_text_color(s_lbl_key_val, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_text_font(s_lbl_key_val, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_key_val, LV_ALIGN_TOP_LEFT, 0, 175);

    // CoreIoT Warning Status Card
    lv_obj_t *warn_title = lv_label_create(left_panel);
    lv_label_set_text(warn_title, LV_SYMBOL_WARNING " RULE-CHAIN WARNING");
    lv_obj_set_style_text_color(warn_title, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_text_font(warn_title, &lv_font_montserrat_14, 0);
    lv_obj_align(warn_title, LV_ALIGN_TOP_LEFT, 0, 202);

    s_warning_badge = lv_obj_create(left_panel);
    lv_obj_set_size(s_warning_badge, 236, 64);
    lv_obj_align(s_warning_badge, LV_ALIGN_TOP_LEFT, 0, 226);
    lv_obj_set_style_bg_color(s_warning_badge, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_bg_opa(s_warning_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_warning_badge, 8, 0);
    lv_obj_set_style_border_width(s_warning_badge, 0, 0);
    lv_obj_set_style_pad_all(s_warning_badge, 6, 0);

    s_lbl_warning_status = lv_label_create(s_warning_badge);
    lv_label_set_text(s_lbl_warning_status, LV_SYMBOL_REFRESH " WAITING FOR SERVER\nNo telemetry received");
    lv_obj_set_style_text_color(s_lbl_warning_status, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_lbl_warning_status, &lv_font_montserrat_14, 0);
    lv_obj_center(s_lbl_warning_status);

    // Interactive Action Buttons (Touch size >= 48x48px)
    lv_obj_t *btn_show_key = lv_button_create(left_panel);
    lv_obj_set_size(btn_show_key, 110, 48);
    lv_obj_align(btn_show_key, LV_ALIGN_BOTTOM_LEFT, 0, -10);
    lv_obj_set_style_bg_color(btn_show_key, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(btn_show_key, 8, 0);
    lv_obj_add_event_cb(btn_show_key, show_hide_key_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_show_key = lv_label_create(btn_show_key);
    lv_label_set_text(lbl_show_key, LV_SYMBOL_EYE_OPEN " Key");
    lv_obj_set_style_text_color(lbl_show_key, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_show_key);

    lv_obj_t *btn_settings = lv_button_create(left_panel);
    lv_obj_set_size(btn_settings, 115, 48);
    lv_obj_align(btn_settings, LV_ALIGN_BOTTOM_RIGHT, 0, -10);
    lv_obj_set_style_bg_color(btn_settings, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_radius(btn_settings, 8, 0);
    lv_obj_add_event_cb(btn_settings, open_settings_modal_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_settings = lv_label_create(btn_settings);
    lv_label_set_text(lbl_settings, LV_SYMBOL_SETTINGS " Config");
    lv_obj_set_style_text_color(lbl_settings, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_settings);

    /* ===================================================================
     * 3. RIGHT PANEL - MAIN TELEMETRY DATA & ACTIONS (510 x 415 px)
     * =================================================================== */
    lv_obj_t *right_panel = lv_obj_create(scr);
    lv_obj_set_size(right_panel, 510, 415);
    lv_obj_align(right_panel, LV_ALIGN_TOP_LEFT, 280, 58);
    lv_obj_set_style_bg_color(right_panel, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(right_panel, 10, 0);
    lv_obj_set_style_border_width(right_panel, 0, 0);
    lv_obj_set_style_pad_all(right_panel, 10, 0);

    // Telemetry Table Header Row
    lv_obj_t *tbl_header = lv_obj_create(right_panel);
    lv_obj_set_size(tbl_header, 490, 32);
    lv_obj_align(tbl_header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(tbl_header, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_radius(tbl_header, 6, 0);
    lv_obj_set_style_border_width(tbl_header, 0, 0);
    lv_obj_set_style_pad_all(tbl_header, 4, 0);

    const char *headers[] = {"Topic / Parameter", "Value", "Unit", "Updated"};
    const int col_x[] = {8, 170, 290, 370};

    for (int i = 0; i < 4; i++) {
        lv_obj_t *h_lbl = lv_label_create(tbl_header);
        lv_label_set_text(h_lbl, headers[i]);
        lv_obj_set_style_text_color(h_lbl, lv_color_hex(0x94A3B8), 0);
        lv_obj_set_style_text_font(h_lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(h_lbl, LV_ALIGN_LEFT_MID, col_x[i], 0);
    }

    // Telemetry Rows (4 rows with alternating background colors #1E293B & #334155)
    const char *params[] = {"Temperature", "Humidity", "Distance", "Relay Status"};
    const char *init_vals[] = {"--", "--", "--", "--"};
    const char *units[] = {"°C", "%", "cm", "-"};
    const char *times[] = {"Waiting...", "Waiting...", "Waiting...", "Waiting..."};

    for (int i = 0; i < 4; i++) {
        uint32_t bg = (i % 2 == 0) ? 0x1E293B : 0x334155;
        s_rows[i].bg_hex = bg;

        lv_obj_t *row = lv_obj_create(right_panel);
        s_rows[i].row_container = row;
        lv_obj_set_size(row, 490, 46);
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 38 + (i * 50));
        lv_obj_set_style_bg_color(row, lv_color_hex(bg), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 4, 0);

        s_rows[i].lbl_param = lv_label_create(row);
        lv_label_set_text(s_rows[i].lbl_param, params[i]);
        lv_obj_set_style_text_color(s_rows[i].lbl_param, lv_color_hex(0xF8FAFC), 0);
        lv_obj_set_style_text_font(s_rows[i].lbl_param, &lv_font_montserrat_14, 0);
        lv_obj_align(s_rows[i].lbl_param, LV_ALIGN_LEFT_MID, 8, 0);

        s_rows[i].lbl_val = lv_label_create(row);
        lv_label_set_text(s_rows[i].lbl_val, init_vals[i]);
        lv_obj_set_style_text_color(s_rows[i].lbl_val, lv_color_hex(0x3B82F6), 0);
        lv_obj_set_style_text_font(s_rows[i].lbl_val, &lv_font_montserrat_20, 0);
        lv_obj_align(s_rows[i].lbl_val, LV_ALIGN_LEFT_MID, 170, 0);

        s_rows[i].lbl_unit = lv_label_create(row);
        lv_label_set_text(s_rows[i].lbl_unit, units[i]);
        lv_obj_set_style_text_color(s_rows[i].lbl_unit, lv_color_hex(0x94A3B8), 0);
        lv_obj_set_style_text_font(s_rows[i].lbl_unit, &lv_font_montserrat_14, 0);
        lv_obj_align(s_rows[i].lbl_unit, LV_ALIGN_LEFT_MID, 290, 0);

        s_rows[i].lbl_updated = lv_label_create(row);
        lv_label_set_text(s_rows[i].lbl_updated, times[i]);
        lv_obj_set_style_text_color(s_rows[i].lbl_updated, lv_color_hex(0x94A3B8), 0);
        lv_obj_set_style_text_font(s_rows[i].lbl_updated, &lv_font_montserrat_14, 0);
        lv_obj_align(s_rows[i].lbl_updated, LV_ALIGN_LEFT_MID, 370, 0);
    }

    /* ===================================================================
     * 4. BOTTOM QUICK ACTIONS & MINI SPARKLINE CHART (510 x 150 px)
     * =================================================================== */
    lv_obj_t *btn_pub = lv_button_create(right_panel);
    lv_obj_set_size(btn_pub, 120, 48);
    lv_obj_align(btn_pub, LV_ALIGN_TOP_LEFT, 0, 245);
    lv_obj_set_style_bg_color(btn_pub, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_radius(btn_pub, 8, 0);
    lv_obj_add_event_cb(btn_pub, publish_test_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_pub = lv_label_create(btn_pub);
    lv_label_set_text(lbl_pub, "Publish Test");
    lv_obj_set_style_text_color(lbl_pub, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_pub);

    lv_obj_t *btn_clear = lv_button_create(right_panel);
    lv_obj_set_size(btn_clear, 115, 48);
    lv_obj_align(btn_clear, LV_ALIGN_TOP_LEFT, 130, 245);
    lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(btn_clear, 8, 0);
    lv_obj_add_event_cb(btn_clear, clear_logs_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_clear = lv_label_create(btn_clear);
    lv_label_set_text(lbl_clear, "Clear Logs");
    lv_obj_set_style_text_color(lbl_clear, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_clear);

    lv_obj_t *btn_recon = lv_button_create(right_panel);
    lv_obj_set_size(btn_recon, 115, 48);
    lv_obj_align(btn_recon, LV_ALIGN_TOP_LEFT, 255, 245);
    lv_obj_set_style_bg_color(btn_recon, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_radius(btn_recon, 8, 0);
    lv_obj_add_event_cb(btn_recon, reconnect_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_recon = lv_label_create(btn_recon);
    lv_label_set_text(lbl_recon, "Reconnect");
    lv_obj_set_style_text_color(lbl_recon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_recon);

    // Mini Sparkline Chart (10-point parameter history)
    s_chart = lv_chart_create(right_panel);
    lv_obj_set_size(s_chart, 490, 85);
    lv_obj_align(s_chart, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_chart, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(s_chart, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_border_width(s_chart, 1, 0);
    lv_obj_set_style_radius(s_chart, 6, 0);

    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, 10);
    s_chart_series = lv_chart_add_series(s_chart, lv_color_hex(0x3B82F6), LV_CHART_AXIS_PRIMARY_Y);

    for (int i = 0; i < 10; i++) {
        lv_chart_set_next_value(s_chart, s_chart_series, 0);
    }
    lv_chart_refresh(s_chart);
    lv_chart_refresh(s_chart);

    ESP_LOGI(TAG, "Control & Monitoring Dashboard UI initialized successfully");
}

void ui_app_update_telemetry(int row_idx, const char *val, const char *updated)
{
    if (row_idx < 0 || row_idx >= 4) return;

    if (s_rows[row_idx].lbl_val != NULL && val != NULL) {
        lv_label_set_text(s_rows[row_idx].lbl_val, val);
    }
    if (s_rows[row_idx].lbl_updated != NULL && updated != NULL) {
        lv_label_set_text(s_rows[row_idx].lbl_updated, updated);
    }

    // Trigger 300ms soft glow flash animation on row update
    if (s_rows[row_idx].row_container != NULL) {
        ui_anim_row_flash(s_rows[row_idx].row_container, s_rows[row_idx].bg_hex);
    }
}

void ui_app_update_mqtt_status(bool connected, const char *broker)
{
    if (s_lbl_mqtt_status == NULL) return;

    char buf[64];
    if (connected) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_BULLET " MQTT: CONNECTED at %s", broker ? broker : "broker.coreiot.io");
        lv_obj_set_style_text_color(s_lbl_mqtt_status, lv_color_hex(0x10B981), 0);
    } else {
        snprintf(buf, sizeof(buf), LV_SYMBOL_BULLET " MQTT: DISCONNECTED (%s)", broker ? broker : "offline");
        lv_obj_set_style_text_color(s_lbl_mqtt_status, lv_color_hex(0xEF4444), 0);
    }
    lv_label_set_text(s_lbl_mqtt_status, buf);
}

void ui_app_add_sparkline_point(int32_t val)
{
    if (s_chart != NULL && s_chart_series != NULL) {
        lv_chart_set_next_value(s_chart, s_chart_series, val);
        lv_chart_refresh(s_chart);
    }
}

void ui_app_update_wifi_info(const char *ssid, const char *ip)
{
    if (s_lbl_wifi == NULL) return;

    char buf[64];
    if (ip && strlen(ip) > 0) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " Wi-Fi: %s | %s", ssid ? ssid : "ACLAB", ip);
        lv_obj_set_style_text_color(s_lbl_wifi, lv_color_hex(0xF8FAFC), 0);
    } else {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " Wi-Fi: %s (Connecting...)", ssid ? ssid : "ACLAB");
        lv_obj_set_style_text_color(s_lbl_wifi, lv_color_hex(0x94A3B8), 0);
    }
    lv_label_set_text(s_lbl_wifi, buf);
}

void ui_app_update_warning_status(const char *status, float distance, bool vehicle_detected)
{
    if (s_warning_badge == NULL || s_lbl_warning_status == NULL) return;

    char computed_status[32] = "SAFE";
    if (status != NULL && strlen(status) > 0) {
        snprintf(computed_status, sizeof(computed_status), "%s", status);
    } else if (distance >= 0.0f) {
        if (distance <= 20.0f) {
            snprintf(computed_status, sizeof(computed_status), "DANGER");
        } else if (distance <= 50.0f) {
            snprintf(computed_status, sizeof(computed_status), "WARNING");
        } else if (distance <= 100.0f) {
            snprintf(computed_status, sizeof(computed_status), "APPROACHING");
        } else {
            snprintf(computed_status, sizeof(computed_status), "SAFE");
        }
    }

    uint32_t bg_color = 0x10B981; // Green
    char txt_buf[128];

    if (strcasecmp(computed_status, "DANGER") == 0 || strcasecmp(computed_status, "CRITICAL") == 0) {
        bg_color = 0xEF4444; // Crimson Red
        if (distance >= 0.0f) {
            snprintf(txt_buf, sizeof(txt_buf), LV_SYMBOL_WARNING " CRITICAL DANGER!\nDistance: %.1f cm", distance);
        } else {
            snprintf(txt_buf, sizeof(txt_buf), LV_SYMBOL_WARNING " CRITICAL DANGER!\nVehicle Too Close");
        }
    } else if (strcasecmp(computed_status, "WARNING") == 0) {
        bg_color = 0xF97316; // Vivid Orange
        if (distance >= 0.0f) {
            snprintf(txt_buf, sizeof(txt_buf), LV_SYMBOL_WARNING " WARNING ALERT\nDistance: %.1f cm", distance);
        } else {
            snprintf(txt_buf, sizeof(txt_buf), LV_SYMBOL_WARNING " WARNING ALERT\nVehicle Detected");
        }
    } else if (strcasecmp(computed_status, "APPROACHING") == 0) {
        bg_color = 0xF59E0B; // Amber
        if (distance >= 0.0f) {
            snprintf(txt_buf, sizeof(txt_buf), LV_SYMBOL_BELL " APPROACHING\nDistance: %.1f cm", distance);
        } else {
            snprintf(txt_buf, sizeof(txt_buf), LV_SYMBOL_BELL " APPROACHING\nVehicle Approaching");
        }
    } else {
        bg_color = 0x10B981; // Emerald Green
        if (distance >= 0.0f) {
            snprintf(txt_buf, sizeof(txt_buf), LV_SYMBOL_OK " SAFE / NORMAL\nDistance: %.1f cm", distance);
        } else {
            snprintf(txt_buf, sizeof(txt_buf), LV_SYMBOL_OK " SAFE / NORMAL\nClear / No Vehicle");
        }
    }

    lv_obj_set_style_bg_color(s_warning_badge, lv_color_hex(bg_color), 0);
    lv_label_set_text(s_lbl_warning_status, txt_buf);
    lv_obj_center(s_lbl_warning_status);
}

