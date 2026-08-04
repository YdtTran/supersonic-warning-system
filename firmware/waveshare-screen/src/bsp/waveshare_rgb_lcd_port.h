/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "esp_lv_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

// Screen Resolution
#define EXAMPLE_LCD_H_RES (800)
#define EXAMPLE_LCD_V_RES (480)

// RGB Panel Timing & Clock
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (16 * 1000 * 1000)
#define EXAMPLE_RGB_BIT_PER_PIXEL (16)
#define EXAMPLE_RGB_DATA_WIDTH (16)
#define EXAMPLE_RGB_BOUNCE_BUFFER_SIZE (EXAMPLE_LCD_H_RES * 10 * EXAMPLE_RGB_DATA_WIDTH / 8)

// RGB LCD Signal Pins
#define EXAMPLE_LCD_IO_RGB_DISP (GPIO_NUM_NC)
#define EXAMPLE_LCD_IO_RGB_VSYNC (GPIO_NUM_3)
#define EXAMPLE_LCD_IO_RGB_HSYNC (GPIO_NUM_46)
#define EXAMPLE_LCD_IO_RGB_DE (GPIO_NUM_5)
#define EXAMPLE_LCD_IO_RGB_PCLK (GPIO_NUM_7)

// RGB 16-bit Data Pins
#define EXAMPLE_LCD_IO_RGB_DATA0 (GPIO_NUM_14)
#define EXAMPLE_LCD_IO_RGB_DATA1 (GPIO_NUM_38)
#define EXAMPLE_LCD_IO_RGB_DATA2 (GPIO_NUM_18)
#define EXAMPLE_LCD_IO_RGB_DATA3 (GPIO_NUM_17)
#define EXAMPLE_LCD_IO_RGB_DATA4 (GPIO_NUM_10)
#define EXAMPLE_LCD_IO_RGB_DATA5 (GPIO_NUM_39)
#define EXAMPLE_LCD_IO_RGB_DATA6 (GPIO_NUM_0)
#define EXAMPLE_LCD_IO_RGB_DATA7 (GPIO_NUM_45)
#define EXAMPLE_LCD_IO_RGB_DATA8 (GPIO_NUM_48)
#define EXAMPLE_LCD_IO_RGB_DATA9 (GPIO_NUM_47)
#define EXAMPLE_LCD_IO_RGB_DATA10 (GPIO_NUM_21)
#define EXAMPLE_LCD_IO_RGB_DATA11 (GPIO_NUM_1)
#define EXAMPLE_LCD_IO_RGB_DATA12 (GPIO_NUM_2)
#define EXAMPLE_LCD_IO_RGB_DATA13 (GPIO_NUM_42)
#define EXAMPLE_LCD_IO_RGB_DATA14 (GPIO_NUM_41)
#define EXAMPLE_LCD_IO_RGB_DATA15 (GPIO_NUM_40)

// Touch GT911 Controller Pins
#define CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911 1
#define EXAMPLE_PIN_NUM_TOUCH_RST (GPIO_NUM_NC)
#define EXAMPLE_PIN_NUM_TOUCH_INT (GPIO_NUM_NC)

// CH422G I2C Configuration
#define I2C_MASTER_SCL_IO (GPIO_NUM_9)
#define I2C_MASTER_SDA_IO (GPIO_NUM_8)
#define I2C_MASTER_NUM (I2C_NUM_0)
#define I2C_MASTER_FREQ_HZ (400 * 1000)
#define I2C_MASTER_TIMEOUT_MS (1000)

#define GPIO_INPUT_PIN_SEL (1ULL << EXAMPLE_TOUCH_RESET_GPIO)
#define EXAMPLE_TOUCH_RESET_GPIO (GPIO_NUM_4)

/**
 * @brief Initialize CH422G I2C IO expander and turn on LCD backlight
 */
esp_err_t waveshare_rgb_lcd_backlight_on(void);

/**
 * @brief Initialize RGB LCD Panel Driver and GT911 Touch Controller
 */
esp_err_t waveshare_esp32_s3_rgb_lcd_init(esp_lv_adapter_tear_avoid_mode_t tear_mode,
                                          esp_lv_adapter_rotation_t rotation,
                                          esp_lcd_panel_handle_t *panel_handle,
                                          esp_lcd_touch_handle_t *touch_handle);

#ifdef __cplusplus
}
#endif
