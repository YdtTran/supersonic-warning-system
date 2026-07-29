/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts an infinite top-to-bottom Y-axis movement animation for the given LVGL object.
 * 
 * @param obj Target LVGL object (e.g., label)
 * @param start_y Starting Y coordinate (e.g. -60)
 * @param end_y Ending Y coordinate (e.g. 480)
 * @param duration_ms Duration of one top-to-bottom cycle in milliseconds (e.g. 3500)
 */
void ui_anim_start_top_to_bottom(lv_obj_t *obj, int32_t start_y, int32_t end_y, uint32_t duration_ms);

/**
 * @brief Triggers a 300ms soft glow/flash animation on a row or card container when new data arrives.
 * 
 * @param obj Target row/card object
 * @param original_bg_hex Original background color hex (e.g. 0x1E293B) to settle back into
 */
void ui_anim_row_flash(lv_obj_t *obj, uint32_t original_bg_hex);

#ifdef __cplusplus
}
#endif

