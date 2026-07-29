/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include "ui_anim.h"

static void anim_y_cb(void *var, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)var, v);
}

void ui_anim_start_top_to_bottom(lv_obj_t *obj, int32_t start_y, int32_t end_y, uint32_t duration_ms)
{
    if (obj == NULL) {
        return;
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, start_y, end_y);
    lv_anim_set_duration(&a, duration_ms);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void row_flash_anim_cb(void *var, int32_t v)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    uint32_t orig_hex = (uint32_t)(uintptr_t)lv_obj_get_user_data(obj);
    lv_color_t original_color = lv_color_hex(orig_hex);
    lv_color_t flash_color = lv_color_hex(0x3B82F6);
    lv_color_t mixed = lv_color_mix(flash_color, original_color, (uint8_t)((v * 255) / 100));
    lv_obj_set_style_bg_color(obj, mixed, 0);
}

void ui_anim_row_flash(lv_obj_t *obj, uint32_t original_bg_hex)
{
    if (obj == NULL) return;

    lv_obj_set_user_data(obj, (void *)(uintptr_t)original_bg_hex);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 100, 0);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_exec_cb(&a, row_flash_anim_cb);
    lv_anim_start(&a);
}

