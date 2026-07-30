#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "app_network.h"

// ==========================================
// Cấu hình UART & Chân GPIO (Cấu hình theo YOLO UNO / ESP32-S3)
// ==========================================
#define UART_PORT_NUM      UART_NUM_1
#define TXD_PIN            (GPIO_NUM_38)
#define RXD_PIN            (GPIO_NUM_21)
#define BUF_SIZE           (256)

static const char *TAG = "JSN_SR04T";

// ==========================================
// Khởi tạo UART
// ==========================================
void init_uart(void) {
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Cài đặt cấu hình UART
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    // Đặt chân RX và TX
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // Cài đặt driver cho UART (dùng bộ đệm nhận RX)
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE, 0, 0, NULL, 0));
}

// ==========================================
// Hàm so sánh để sắp xếp mảng (qsort)
// ==========================================
int compare_ints(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

// ==========================================
// Hàm đọc một gói dữ liệu từ UART (4 byte)
// Tra ve: Khoảng cách (mm) hoặc -1 nếu không hợp lệ
// ==========================================
int doc_uart(void) {
    uint8_t head;
    
    // Đọc từng byte để tìm Header (0xFF)
    while (uart_read_bytes(UART_PORT_NUM, &head, 1, pdMS_TO_TICKS(10)) > 0) {
        if (head == 0xFF) {
            uint8_t data[3];
            // Đọc 3 byte tiếp theo: High Byte, Low Byte, Checksum
            int len = uart_read_bytes(UART_PORT_NUM, data, 3, pdMS_TO_TICKS(50));
            if (len == 3) {
                uint8_t buf[4] = {0xFF, data[0], data[1], data[2]};
                
                // Tính checksum: (0xFF + Data_H + Data_L) & 0xFF
                uint8_t checksum = (buf[0] + buf[1] + buf[2]) & 0xFF;

                if (buf[3] == checksum) {
                    int distance_mm = (buf[1] << 8) + buf[2];
                    
                    // Lọc sơ bộ khoảng cách hợp lý (20cm - 600cm)
                    if (distance_mm >= 200 && distance_mm <= 6000) {
                        return distance_mm;
                    }
                }
            }
        }
    }
    return -1; // Dữ liệu không hợp lệ hoặc hết Timeout
}

// ==========================================
// Task chính xử lý đo đạc & gửi dữ liệu CoreIoT
// ==========================================
void main_task(void *pvParameters) {
    ESP_LOGI(TAG, "JSN-SR04T Mode 1 started (UART auto-measure & CoreIoT Telemetry)");
    uint32_t loop_count = 0;

    while (1) {
        loop_count++;
        int samples[5];
        int valid_count = 0;

        // --- Lấy 5 mẫu, mỗi mẫu cách nhau 100ms (Tổng 500ms) ---
        for (int i = 0; i < 5; i++) {
            int dist = doc_uart();
            if (dist != -1) {
                samples[valid_count++] = dist;
            }
            vTaskDelay(pdMS_TO_TICKS(100)); // Tương đương await asleep_ms(100)
        }

        // --- Lọc ngoại lệ và tính trung bình ---
        if (valid_count >= 3) {
            // Sắp xếp mảng để lọc Min và Max
            qsort(samples, valid_count, sizeof(int), compare_ints);

            // Bỏ phần tử nhỏ nhất (samples[0]) và lớn nhất (samples[valid_count - 1])
            int sum = 0;
            for (int i = 1; i < valid_count - 1; i++) {
                sum += samples[i];
            }

            float avg_mm = (float)sum / (valid_count - 2);
            float dist_cm = avg_mm / 10.0f; // Chuyển mm sang cm
            bool check_element = (dist_cm > 0.0f && dist_cm <= 50.0f);
            const char *reset_reason = get_reset_reason_string();

            ESP_LOGI(TAG, "[LOOP #%lu] Distance: %.2f mm (%.1f cm) | Object Near: %s | Reset Reason: %s",
                     (unsigned long)loop_count, avg_mm, dist_cm,
                     check_element ? "YES" : "NO", reset_reason);

            // Đóng gói JSON telemetry gửi CoreIoT & Waveshare Screen
            char payload[384];
            snprintf(payload, sizeof(payload),
                     "{"
                     "\"distance\":%.1f,"
                     "\"distance_cm\":%.1f,"
                     "\"reboot-reason\":\"%s\","
                     "\"reboot_reason\":\"%s\","
                     "\"check-element\":%s,"
                     "\"check_element\":%s,"
                     "\"vehicle_detected\":%s,"
                     "\"device-id\":\"%s\","
                     "\"device_id\":\"%s\""
                     "}",
                     dist_cm, dist_cm,
                     reset_reason, reset_reason,
                     check_element ? "true" : "false", check_element ? "true" : "false",
                     check_element ? "true" : "false",
                     CONFIG_DEVICE_ID, CONFIG_DEVICE_ID);

            int msg_id = app_network_publish_telemetry(payload);
            if (msg_id >= 0) {
                ESP_LOGI(TAG, "[MQTT PUB SUCCESS] Sent telemetry (msg_id=%d): %s", msg_id, payload);
            } else {
                ESP_LOGD(TAG, "[MQTT] Network connecting...");
            }
        } else {
            ESP_LOGW(TAG, "[LOOP #%lu] Not enough valid samples (%d/5 valid). Check sensor power & RX pin GPIO%d!",
                     (unsigned long)loop_count, valid_count, RXD_PIN);
        }

        fflush(stdout);
        // Tạm dừng 500ms nữa để đạt đúng chu kỳ 1.0 giây gửi 1 lần (500ms lấy mẫu + 500ms nghỉ = 1000ms)
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ==========================================
// Hàm main (chương trình chính)
// ==========================================
void app_main(void) {
    ESP_LOGI(TAG, "Initializing Sensor Node Application...");

    // Khởi tạo mạng (Wi-Fi + MQTT CoreIoT)
    app_network_init();

    // Khởi tạo UART cho cảm biến JSN-SR04T
    init_uart();

    // Tạo FreeRTOS task để chạy loop
    xTaskCreate(main_task, "main_task", 4096, NULL, 5, NULL);
}