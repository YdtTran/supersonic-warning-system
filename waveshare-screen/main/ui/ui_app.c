/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include "ui_app.h"
#include "ui_anim.h"
#include "esp_log.h"
#include "waveshare_rgb_lcd_port.h"

static const char *TAG = "ui_app";

void ui_app_init(void)
{
    // Configure dark slate background (#0F172A)
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Create label for "ACLAB 2023"
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "ACLAB 2023");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x38BDF8), 0); // Sky Blue glow

    // Align horizontally at top center
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, -60);

    // Start top-to-bottom animation (3.5s per loop)
    ui_anim_start_top_to_bottom(label, -60, EXAMPLE_LCD_V_RES, 3500);

    ESP_LOGI(TAG, "ACLAB 2023 UI initialized successfully");
}
