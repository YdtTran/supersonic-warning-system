#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"

static const char *TAG = "SENSOR_NODE";

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (4) // GPIO4 for PWM Output Test
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT
#define LEDC_DUTY               (4095) // 50% duty cycle (2^13 - 1 = 8191)
#define LEDC_FREQUENCY          (5000) // 5 kHz PWM

static void init_ledc_pwm(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // Set duty to 0 initially
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ESP_LOGI(TAG, "LEDC PWM initialized on GPIO%d at %d Hz", LEDC_OUTPUT_IO, LEDC_FREQUENCY);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   ESP32-S3 Sensor Node Application Starting      ");
    ESP_LOGI(TAG, "==================================================");

    // Initialize NVS Storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS Flash Initialized");

    // Initialize LEDC PWM Peripheral
    init_ledc_pwm();

    // Fade LEDC Duty Cycle in a loop
    int fade_dir = 1;
    uint32_t current_duty = 0;

    while (1) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, current_duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

        if (fade_dir) {
            current_duty += 250;
            if (current_duty >= LEDC_DUTY) {
                current_duty = LEDC_DUTY;
                fade_dir = 0;
            }
        } else {
            if (current_duty <= 250) {
                current_duty = 0;
                fade_dir = 1;
            } else {
                current_duty -= 250;
            }
        }

        int64_t uptime_ms = esp_timer_get_time() / 1000;
        ESP_LOGI(TAG, "Heartbeat | Uptime: %lld ms | PWM Duty: %lu", uptime_ms, (unsigned long)current_duty);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
