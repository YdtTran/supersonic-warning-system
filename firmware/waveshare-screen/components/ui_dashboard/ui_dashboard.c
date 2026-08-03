/*
 * SPDX-FileCopyrightText: 2026 Vehicle Warning System
 * SPDX-License-Identifier: MIT
 */

#include "ui_dashboard.h"
#include "sensor_model.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ui_dashboard";

#define COLOR_BG          0x0F172A
#define COLOR_PANEL       0x1E293B
#define COLOR_BORDER      0x334155
#define COLOR_ACCENT      0x38BDF8
#define COLOR_TEXT        0xF8FAFC
#define COLOR_SAFE        0x00C853
#define COLOR_CAUTION     0xFFD600
#define COLOR_DANGER      0xFF1744

// Fast-change threshold (cm) used by the crossing-traffic hazard heuristic.
#define CROSSING_DELTA_CM 40
#define CROSSING_FRONT_THRESHOLD_CM 150

typedef struct {
    lv_obj_t *arc;
    int16_t local_x;
    int16_t local_y;
    int16_t mid_angle_deg; // LVGL angle convention: 0=right, 90=down, 180=left, 270=up
    lv_anim_t blink_anim;
    bool blink_running;
} sensor_arc_t;

typedef struct {
    lv_obj_t *row_value_lbl;
} sensor_row_t;

static lv_obj_t *s_screen;
static lv_obj_t *s_tab_btn_collision;
static lv_obj_t *s_tab_btn_system;
static lv_obj_t *s_page_collision;
static lv_obj_t *s_page_system;
static lv_obj_t *s_lbl_wifi_status;
static lv_obj_t *s_lbl_mqtt_status;
static lv_obj_t *s_lbl_hazard_overall;
static lv_obj_t *s_lbl_crossing_risk;
static lv_obj_t *s_lbl_relay_state;
static lv_obj_t *s_lbl_sys_wifi;
static lv_obj_t *s_lbl_sys_mqtt;
static lv_obj_t *s_lbl_sys_device;
static bool s_alarm_muted = false;

static sensor_arc_t s_arcs[SENSOR_MODEL_COUNT];
static sensor_row_t s_rows[SENSOR_MODEL_COUNT];
static uint16_t s_prev_distance_cm[SENSOR_MODEL_COUNT];
static bool s_forced_crossing_warning = false;

static const char *k_sensor_labels[SENSOR_MODEL_COUNT] = {
    "S1 (Front)", "S2 (Rear)", "S3 (L-Front)", "S4 (L-Rear)", "S5 (R-Front)", "S6 (R-Rear)",
};

static lv_color_t zone_color(sensor_zone_t zone)
{
    switch (zone) {
        case SENSOR_ZONE_DANGER: return lv_color_hex(COLOR_DANGER);
        case SENSOR_ZONE_CAUTION: return lv_color_hex(COLOR_CAUTION);
        default: return lv_color_hex(COLOR_SAFE);
    }
}

static void blink_anim_cb(void *var, int32_t value)
{
    lv_obj_set_style_arc_opa((lv_obj_t *)var, (lv_opa_t)value, LV_PART_INDICATOR);
}

static void arc_set_zone(sensor_arc_t *a, sensor_zone_t zone)
{
    lv_obj_set_style_arc_color(a->arc, zone_color(zone), LV_PART_INDICATOR);

    if (a->blink_running) {
        lv_anim_delete(a->arc, blink_anim_cb);
        a->blink_running = false;
        lv_obj_set_style_arc_opa(a->arc, LV_OPA_COVER, LV_PART_INDICATOR);
    }

    if (zone == SENSOR_ZONE_DANGER && !s_alarm_muted) {
        lv_anim_init(&a->blink_anim);
        lv_anim_set_var(&a->blink_anim, a->arc);
        lv_anim_set_exec_cb(&a->blink_anim, blink_anim_cb);
        lv_anim_set_values(&a->blink_anim, LV_OPA_COVER, LV_OPA_30);
        lv_anim_set_time(&a->blink_anim, 400);
        lv_anim_set_playback_time(&a->blink_anim, 400);
        lv_anim_set_repeat_count(&a->blink_anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a->blink_anim);
        a->blink_running = true;
    } else {
        lv_obj_set_style_arc_opa(a->arc, zone == SENSOR_ZONE_SAFE ? (LV_OPA_60) : (LV_OPA_80), LV_PART_INDICATOR);
    }
}

static lv_obj_t *make_arc(lv_obj_t *parent, int16_t local_x, int16_t local_y, int16_t mid_angle_deg)
{
    const int32_t radius = 90;
    const int32_t half_fov = 37; // ~75deg / 2

    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, radius, radius);
    lv_obj_set_pos(arc, local_x - radius / 2, local_y - radius / 2);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    int32_t start = mid_angle_deg - half_fov;
    int32_t end = mid_angle_deg + half_fov;
    while (start < 0) start += 360;
    while (end < 0) end += 360;
    start %= 360;
    end %= 360;

    lv_arc_set_bg_angles(arc, start, end);
    lv_arc_set_angles(arc, start, end);
    lv_obj_set_style_arc_width(arc, 24, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 24, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(COLOR_SAFE), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_60, LV_PART_INDICATOR);

    return arc;
}

static void build_header(lv_obj_t *parent)
{
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_set_size(header, LV_PCT(100), 40);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(header, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_pad_hor(header, 12, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_WARNING " CoreIoT Dashboard");
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    s_tab_btn_collision = lv_btn_create(header);
    lv_obj_set_size(s_tab_btn_collision, 100, 28);
    lv_obj_align(s_tab_btn_collision, LV_ALIGN_CENTER, -55, 0);
    lv_obj_t *lbl1 = lv_label_create(s_tab_btn_collision);
    lv_label_set_text(lbl1, "COLLISION");
    lv_obj_center(lbl1);

    s_tab_btn_system = lv_btn_create(header);
    lv_obj_set_size(s_tab_btn_system, 100, 28);
    lv_obj_align(s_tab_btn_system, LV_ALIGN_CENTER, 55, 0);
    lv_obj_t *lbl2 = lv_label_create(s_tab_btn_system);
    lv_label_set_text(lbl2, "SYSTEM");
    lv_obj_center(lbl2);

    s_lbl_wifi_status = lv_label_create(header);
    lv_label_set_text(s_lbl_wifi_status, LV_SYMBOL_WIFI " --");
    lv_obj_set_style_text_color(s_lbl_wifi_status, lv_color_hex(COLOR_DANGER), 0);
    lv_obj_align(s_lbl_wifi_status, LV_ALIGN_RIGHT_MID, -70, 0);

    s_lbl_mqtt_status = lv_label_create(header);
    lv_label_set_text(s_lbl_mqtt_status, "MQTT: DOWN");
    lv_obj_set_style_text_color(s_lbl_mqtt_status, lv_color_hex(COLOR_DANGER), 0);
    lv_obj_align(s_lbl_mqtt_status, LV_ALIGN_RIGHT_MID, 0, 0);
}

static void mute_btn_cb(lv_event_t *e)
{
    (void)e;
    s_alarm_muted = !s_alarm_muted;
    sensor_reading_t readings[SENSOR_MODEL_COUNT];
    sensor_model_get_all(readings);
    for (int i = 0; i < SENSOR_MODEL_COUNT; i++) {
        arc_set_zone(&s_arcs[i], sensor_model_classify(readings[i].distance_cm));
    }
}

static lv_obj_t *build_left_sidebar(lv_obj_t *parent)
{
    lv_obj_t *sidebar = lv_obj_create(parent);
    lv_obj_set_size(sidebar, 180, LV_PCT(100));
    lv_obj_set_style_bg_color(sidebar, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(sidebar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sidebar, 0, 0);
    lv_obj_set_style_radius(sidebar, 0, 0);
    lv_obj_set_style_pad_all(sidebar, 8, 0);
    lv_obj_set_flex_flow(sidebar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sidebar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *hdr = lv_label_create(sidebar);
    lv_label_set_text(hdr, "SENSOR READINGS");
    lv_obj_set_style_text_color(hdr, lv_color_hex(COLOR_ACCENT), 0);

    for (int i = 0; i < SENSOR_MODEL_COUNT; i++) {
        lv_obj_t *row = lv_label_create(sidebar);
        lv_label_set_text_fmt(row, "%s: -- cm", k_sensor_labels[i]);
        lv_obj_set_style_text_color(row, lv_color_hex(COLOR_TEXT), 0);
        s_rows[i].row_value_lbl = row;
    }

    lv_obj_t *action_hdr = lv_label_create(sidebar);
    lv_label_set_text(action_hdr, "ACTIONS");
    lv_obj_set_style_text_color(action_hdr, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_pad_top(action_hdr, 12, 0);

    lv_obj_t *mute_btn = lv_btn_create(sidebar);
    lv_obj_set_size(mute_btn, LV_PCT(100), 32);
    lv_obj_add_event_cb(mute_btn, mute_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *mute_lbl = lv_label_create(mute_btn);
    lv_label_set_text(mute_lbl, "Mute Alarm");
    lv_obj_center(mute_lbl);

    lv_obj_t *calib_btn = lv_btn_create(sidebar);
    lv_obj_set_size(calib_btn, LV_PCT(100), 32);
    lv_obj_t *calib_lbl = lv_label_create(calib_btn);
    lv_label_set_text(calib_lbl, "Calibrate");
    lv_obj_center(calib_lbl);

    return sidebar;
}

static lv_obj_t *build_center_canvas(lv_obj_t *parent)
{
    lv_obj_t *canvas = lv_obj_create(parent);
    lv_obj_set_size(canvas, 440, LV_PCT(100));
    lv_obj_set_style_bg_color(canvas, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(canvas, 0, 0);
    lv_obj_set_style_radius(canvas, 0, 0);
    lv_obj_set_style_pad_all(canvas, 0, 0);
    lv_obj_remove_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *car = lv_obj_create(canvas);
    lv_obj_set_size(car, 160, 260);
    lv_obj_align(car, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(car, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(car, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(car, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_border_width(car, 2, 0);
    lv_obj_set_style_radius(car, 12, 0);
    lv_obj_remove_flag(car, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hood = lv_label_create(car);
    lv_label_set_text(hood, "FRONT HOOD");
    lv_obj_set_style_text_color(hood, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_align(hood, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *cabin = lv_label_create(car);
    lv_label_set_text(cabin, "CABIN");
    lv_obj_set_style_text_color(cabin, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_align(cabin, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *trunk = lv_label_create(car);
    lv_label_set_text(trunk, "REAR TRUNK");
    lv_obj_set_style_text_color(trunk, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_align(trunk, LV_ALIGN_BOTTOM_MID, 0, -8);

    // Sensor beam arcs laid out like the standard truck "No-Zone" diagram:
    // one sensor centered front, one centered rear, two per side (front-half
    // and rear-half of that side). Angle convention: LVGL 0deg=right(3 o'clock),
    // 90=down, 180=left, 270=up. Car body spans local (140,90)-(300,350).
    static const struct {
        int16_t x, y, angle;
    } k_layout[SENSOR_MODEL_COUNT] = {
        [SENSOR_ID_FRONT]       = {220, 60, 270},
        [SENSOR_ID_REAR]        = {220, 380, 90},
        [SENSOR_ID_LEFT_FRONT]  = {90, 150, 180},
        [SENSOR_ID_LEFT_REAR]   = {90, 290, 180},
        [SENSOR_ID_RIGHT_FRONT] = {350, 150, 0},
        [SENSOR_ID_RIGHT_REAR]  = {350, 290, 0},
    };

    for (int i = 0; i < SENSOR_MODEL_COUNT; i++) {
        s_arcs[i].local_x = k_layout[i].x;
        s_arcs[i].local_y = k_layout[i].y;
        s_arcs[i].mid_angle_deg = k_layout[i].angle;
        s_arcs[i].arc = make_arc(canvas, k_layout[i].x, k_layout[i].y, k_layout[i].angle);
        s_arcs[i].blink_running = false;
    }

    return canvas;
}

static lv_obj_t *build_right_sidebar(lv_obj_t *parent)
{
    lv_obj_t *sidebar = lv_obj_create(parent);
    lv_obj_set_size(sidebar, 180, LV_PCT(100));
    lv_obj_set_style_bg_color(sidebar, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(sidebar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sidebar, 0, 0);
    lv_obj_set_style_radius(sidebar, 0, 0);
    lv_obj_set_style_pad_all(sidebar, 8, 0);
    lv_obj_set_flex_flow(sidebar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sidebar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *hazard_hdr = lv_label_create(sidebar);
    lv_label_set_text(hazard_hdr, "HAZARD STATE");
    lv_obj_set_style_text_color(hazard_hdr, lv_color_hex(COLOR_ACCENT), 0);

    s_lbl_hazard_overall = lv_label_create(sidebar);
    lv_label_set_text(s_lbl_hazard_overall, "OVERALL: SAFE");
    lv_obj_set_style_text_color(s_lbl_hazard_overall, lv_color_hex(COLOR_SAFE), 0);

    lv_obj_t *risk_hdr = lv_label_create(sidebar);
    lv_label_set_text(risk_hdr, "CROSSING RISK");
    lv_obj_set_style_text_color(risk_hdr, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_pad_top(risk_hdr, 12, 0);

    s_lbl_crossing_risk = lv_label_create(sidebar);
    lv_label_set_text(s_lbl_crossing_risk, "None detected");
    lv_obj_set_style_text_color(s_lbl_crossing_risk, lv_color_hex(COLOR_TEXT), 0);
    lv_label_set_long_mode(s_lbl_crossing_risk, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_crossing_risk, LV_PCT(100));

    lv_obj_t *relay_hdr = lv_label_create(sidebar);
    lv_label_set_text(relay_hdr, "SERVER STATUS");
    lv_obj_set_style_text_color(relay_hdr, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_pad_top(relay_hdr, 12, 0);

    s_lbl_relay_state = lv_label_create(sidebar);
    lv_label_set_text(s_lbl_relay_state, "RELAY: -- | --");
    lv_obj_set_style_text_color(s_lbl_relay_state, lv_color_hex(COLOR_TEXT), 0);

    lv_obj_t *legend_hdr = lv_label_create(sidebar);
    lv_label_set_text(legend_hdr, "LEGEND");
    lv_obj_set_style_text_color(legend_hdr, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_pad_top(legend_hdr, 12, 0);

    static const struct { uint32_t color; const char *text; } k_legend[] = {
        {COLOR_SAFE, "> 100cm : Safe"},
        {COLOR_CAUTION, "30-100cm : Caution"},
        {COLOR_DANGER, "< 30cm : Danger"},
    };
    for (size_t i = 0; i < sizeof(k_legend) / sizeof(k_legend[0]); i++) {
        lv_obj_t *lbl = lv_label_create(sidebar);
        lv_label_set_text(lbl, k_legend[i].text);
        lv_obj_set_style_text_color(lbl, lv_color_hex(k_legend[i].color), 0);
    }

    return sidebar;
}

static void set_active_tab(bool collision)
{
    if (s_page_collision) lv_obj_add_flag(s_page_collision, LV_OBJ_FLAG_HIDDEN);
    if (s_page_system) lv_obj_add_flag(s_page_system, LV_OBJ_FLAG_HIDDEN);

    if (collision && s_page_collision) {
        lv_obj_remove_flag(s_page_collision, LV_OBJ_FLAG_HIDDEN);
    } else if (!collision && s_page_system) {
        lv_obj_remove_flag(s_page_system, LV_OBJ_FLAG_HIDDEN);
    }
}

static void tab_collision_cb(lv_event_t *e)
{
    (void)e;
    set_active_tab(true);
}

static void tab_system_cb(lv_event_t *e)
{
    (void)e;
    set_active_tab(false);
}

static lv_obj_t *build_system_page(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(page, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 24, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "SYSTEM STATUS");
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_ACCENT), 0);

    s_lbl_sys_device = lv_label_create(page);
    lv_label_set_text(s_lbl_sys_device, "Device: ESP32-S3 Collision Dashboard");
    lv_obj_set_style_text_color(s_lbl_sys_device, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_pad_top(s_lbl_sys_device, 12, 0);

    s_lbl_sys_wifi = lv_label_create(page);
    lv_label_set_text(s_lbl_sys_wifi, "Wi-Fi: disconnected");
    lv_obj_set_style_text_color(s_lbl_sys_wifi, lv_color_hex(COLOR_TEXT), 0);

    s_lbl_sys_mqtt = lv_label_create(page);
    lv_label_set_text(s_lbl_sys_mqtt, "MQTT/CoreIoT: down");
    lv_obj_set_style_text_color(s_lbl_sys_mqtt, lv_color_hex(COLOR_TEXT), 0);

    return page;
}

void ui_dashboard_init(void)
{
    sensor_model_init();
    memset(s_prev_distance_cm, 0, sizeof(s_prev_distance_cm));

    s_screen = lv_screen_active();
    if (s_screen == NULL) {
        s_screen = lv_obj_create(NULL);
        lv_screen_load(s_screen);
    }
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_header(s_screen);

    lv_obj_t *content = lv_obj_create(s_screen);
    lv_obj_set_size(content, LV_PCT(100), 440);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_style_pad_column(content, 0, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);

    s_page_collision = lv_obj_create(content);
    lv_obj_set_size(s_page_collision, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(s_page_collision, 0, 0);
    lv_obj_set_style_pad_column(s_page_collision, 0, 0);
    lv_obj_set_style_border_width(s_page_collision, 0, 0);
    lv_obj_set_style_bg_opa(s_page_collision, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(s_page_collision, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_page_collision, LV_FLEX_FLOW_ROW);

    build_left_sidebar(s_page_collision);
    build_center_canvas(s_page_collision);
    build_right_sidebar(s_page_collision);

    s_page_system = build_system_page(content);
    lv_obj_add_flag(s_page_system, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(s_tab_btn_collision, tab_collision_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_tab_btn_system, tab_system_cb, LV_EVENT_CLICKED, NULL);

    ESP_LOGI(TAG, "Collision dashboard UI initialized");
}

static void evaluate_hazard(void)
{
    sensor_reading_t readings[SENSOR_MODEL_COUNT];
    sensor_model_get_all(readings);

    sensor_zone_t worst = SENSOR_ZONE_SAFE;
    for (int i = 0; i < SENSOR_MODEL_COUNT; i++) {
        sensor_zone_t z = sensor_model_classify(readings[i].distance_cm);
        if (z > worst) worst = z;
    }

    if (s_lbl_hazard_overall) {
        const char *text = worst == SENSOR_ZONE_DANGER ? "OVERALL: DANGER"
                          : worst == SENSOR_ZONE_CAUTION ? "OVERALL: CAUTION"
                          : "OVERALL: SAFE";
        lv_label_set_text(s_lbl_hazard_overall, text);
        lv_obj_set_style_text_color(s_lbl_hazard_overall, zone_color(worst), 0);
    }

    // Crossing-traffic heuristic: the front sensor is close AND a side sensor's reading is
    // moving fast (large frame-to-frame delta), suggesting cross traffic passing the flank.
    bool front_close = readings[SENSOR_ID_FRONT].distance_cm < CROSSING_FRONT_THRESHOLD_CM;

    bool side_changing_fast = false;
    const char *crossing_sensor = NULL;
    for (int i = SENSOR_ID_LEFT_FRONT; i <= SENSOR_ID_RIGHT_REAR; i++) {
        int32_t delta = (int32_t)readings[i].distance_cm - (int32_t)s_prev_distance_cm[i];
        if (delta < 0) delta = -delta;
        if (delta >= CROSSING_DELTA_CM) {
            side_changing_fast = true;
            crossing_sensor = k_sensor_labels[i];
        }
    }

    bool crossing_hazard = s_forced_crossing_warning || (front_close && side_changing_fast);

    if (s_lbl_crossing_risk) {
        if (crossing_hazard) {
            char buf[64];
            snprintf(buf, sizeof(buf), "CROSSING TRAFFIC HAZARD%s%s",
                     crossing_sensor ? " - " : "", crossing_sensor ? crossing_sensor : "");
            lv_label_set_text(s_lbl_crossing_risk, buf);
            lv_obj_set_style_text_color(s_lbl_crossing_risk, lv_color_hex(COLOR_DANGER), 0);
        } else {
            lv_label_set_text(s_lbl_crossing_risk, "None detected");
            lv_obj_set_style_text_color(s_lbl_crossing_risk, lv_color_hex(COLOR_TEXT), 0);
        }
    }

    for (int i = 0; i < SENSOR_MODEL_COUNT; i++) {
        s_prev_distance_cm[i] = readings[i].distance_cm;
    }
}

void ui_dashboard_update_sensor(uint8_t sensor_id, uint16_t dist_cm)
{
    if (sensor_id >= SENSOR_MODEL_COUNT) {
        return;
    }

    sensor_model_set_distance((sensor_id_t)sensor_id, dist_cm);
    sensor_zone_t zone = sensor_model_classify(dist_cm);

    if (s_rows[sensor_id].row_value_lbl) {
        const char *suffix = zone == SENSOR_ZONE_DANGER ? " DANG" : "";
        lv_label_set_text_fmt(s_rows[sensor_id].row_value_lbl, "%s: %u cm%s",
                               k_sensor_labels[sensor_id], dist_cm, suffix);
        lv_obj_set_style_text_color(s_rows[sensor_id].row_value_lbl, zone_color(zone), 0);
    }

    if (s_arcs[sensor_id].arc) {
        arc_set_zone(&s_arcs[sensor_id], zone);
    }

    evaluate_hazard();
}

void ui_dashboard_set_iot_status(bool is_connected, const char *ip)
{
    if (s_lbl_wifi_status) {
        if (is_connected && ip) {
            lv_label_set_text_fmt(s_lbl_wifi_status, LV_SYMBOL_WIFI " %s", ip);
            lv_obj_set_style_text_color(s_lbl_wifi_status, lv_color_hex(COLOR_SAFE), 0);
        } else {
            lv_label_set_text(s_lbl_wifi_status, LV_SYMBOL_WIFI " --");
            lv_obj_set_style_text_color(s_lbl_wifi_status, lv_color_hex(COLOR_DANGER), 0);
        }
    }

    if (s_lbl_mqtt_status) {
        lv_label_set_text(s_lbl_mqtt_status, is_connected ? "MQTT: UP" : "MQTT: DOWN");
        lv_obj_set_style_text_color(s_lbl_mqtt_status, is_connected ? lv_color_hex(COLOR_SAFE) : lv_color_hex(COLOR_DANGER), 0);
    }

    if (s_lbl_sys_wifi) {
        lv_label_set_text_fmt(s_lbl_sys_wifi, "Wi-Fi: %s", is_connected ? (ip ? ip : "connected") : "disconnected");
    }
    if (s_lbl_sys_mqtt) {
        lv_label_set_text_fmt(s_lbl_sys_mqtt, "MQTT/CoreIoT: %s", is_connected ? "up" : "down");
    }
}

void ui_dashboard_set_hazard_warning(bool is_pedestrian_crossing_risk)
{
    s_forced_crossing_warning = is_pedestrian_crossing_risk;
    evaluate_hazard();
}

void ui_dashboard_set_relay_state(bool relay_on, const char *warning_status)
{
    if (!s_lbl_relay_state) {
        return;
    }

    lv_label_set_text_fmt(s_lbl_relay_state, "RELAY: %s | %s",
                           relay_on ? "ON" : "OFF",
                           warning_status ? warning_status : "--");

    uint32_t color = COLOR_SAFE;
    if (warning_status) {
        if (strcmp(warning_status, "DANGER") == 0) {
            color = COLOR_DANGER;
        } else if (strcmp(warning_status, "WARNING") == 0) {
            color = COLOR_CAUTION;
        }
    }
    lv_obj_set_style_text_color(s_lbl_relay_state, lv_color_hex(color), 0);
}
