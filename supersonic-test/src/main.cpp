#include <Arduino.h>
#include "SoftUART.h"

#define NUM_SENSORS 6

// 6 Chân GPIO RX cho 6 cảm biến
const int SENSOR_RX_PINS[NUM_SENSORS] = {18, 19, 21, 22, 23, 25};
SoftUART sensors[NUM_SENSORS];

// Cấu trúc dữ liệu chứa kết quả đo của 6 cảm biến
typedef struct {
    uint16_t distances_mm[NUM_SENSORS];
    bool valid[NUM_SENSORS];
    uint32_t timestamp;
} SensorBatch_t;

// Queue giao tiếp giữa Core 1 (Đọc sensor) và Core 0 (Hiển thị / Ứng dụng)
QueueHandle_t xSensorQueue = NULL;

// Handle Task
TaskHandle_t TaskSensorHandle = NULL;
TaskHandle_t TaskDisplayHandle = NULL;

// --- TASK 1: ĐỌC 6 CẢM BIẾN BẰNG SOFTWARE UART (CỐ ĐỊNH TRÊN CORE 1) ---
void TaskRead6Sensors(void *pvParameters) {
    Serial.printf("[RTOS] TaskRead6Sensors đang chạy trên CORE %d (Độ ưu tiên cao)\n", xPortGetCoreID());

    // Khởi tạo 6 cổng Software UART
    for (int i = 0; i < NUM_SENSORS; i++) {
        sensors[i].begin(SENSOR_RX_PINS[i], -1, 9600);
    }

    SensorBatch_t batch;

    for (;;) {
        batch.timestamp = millis();

        // Đọc lần lượt 6 cảm biến
        for (int i = 0; i < NUM_SENSORS; i++) {
            uint16_t dist_mm = 0;
            // Đọc gói 4-byte từ SoftUART
            bool ok = sensors[i].readSensorPacket(&dist_mm, 40);
            
            batch.distances_mm[i] = ok ? dist_mm : 0;
            batch.valid[i] = ok;
        }

        // Đẩy kết quả vào FreeRTOS Queue (Non-blocking nếu Queue đầy)
        if (xSensorQueue != NULL) {
            xQueueSend(xSensorQueue, &batch, 0);
        }

        // Nghỉ 50ms giữa các chu kỳ đo (khoảng 20Hz update rate)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- TASK 2: HIỂN THỊ DỮ LIỆU NON-BLOCKING (CỐ ĐỊNH TRÊN CORE 0) ---
void TaskDisplayMain(void *pvParameters) {
    Serial.printf("[RTOS] TaskDisplayMain đang chạy trên CORE %d\n", xPortGetCoreID());

    SensorBatch_t receivedBatch;

    for (;;) {
        // Nhận dữ liệu từ Queue (chờ tối đa 1000ms)
        if (xQueueReceive(xSensorQueue, &receivedBatch, pdMS_TO_TICKS(1000)) == pdTRUE) {
            Serial.printf("\n--- [TIME: %6d ms] KẾT QUẢ ĐỌC 6 CẢM BIẾN ---\n", receivedBatch.timestamp);
            
            for (int i = 0; i < NUM_SENSORS; i++) {
                if (receivedBatch.valid[i]) {
                    float cm = receivedBatch.distances_mm[i] / 10.0f;
                    Serial.printf("  Sensor %d (GPIO %2d): %4d mm (%5.1f cm) [OK]\n", 
                                  i + 1, SENSOR_RX_PINS[i], receivedBatch.distances_mm[i], cm);
                } else {
                    Serial.printf("  Sensor %d (GPIO %2d): TIMEOUT / CHECKSUM ERROR [FAIL]\n", 
                                  i + 1, SENSOR_RX_PINS[i]);
                }
            }
            Serial.println("--------------------------------------------------");
        }
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    Serial.println("\n=======================================================");
    Serial.println("  HỆ THỐNG 6 CẢM BIẾN SIÊU ÂM - SOFTWARE UART DUAL CORE");
    Serial.println("  Core 1: Đọc 6 Software UART (GPIO 18,19,21,22,23,25) ");
    Serial.println("  Core 0: Nhận dữ liệu từ FreeRTOS Queue non-blocking  ");
    Serial.println("=======================================================\n");

    // Tạo Queue chứa tối đa 5 bản tin
    xSensorQueue = xQueueCreate(5, sizeof(SensorBatch_t));

    if (xSensorQueue == NULL) {
        Serial.println("[LỖI] Không thể tạo FreeRTOS Queue!");
        return;
    }

    // Task 1: Đọc Sensor - Cố định trên CORE 1, Priority 5 (Cao)
    xTaskCreatePinnedToCore(
        TaskRead6Sensors,
        "TaskRead6Sensors",
        4096,
        NULL,
        5,
        &TaskSensorHandle,
        1 // CORE 1
    );

    // Task 2: Hiển thị / Ứng dụng - Cố định trên CORE 0, Priority 1
    xTaskCreatePinnedToCore(
        TaskDisplayMain,
        "TaskDisplayMain",
        4096,
        NULL,
        1,
        &TaskDisplayHandle,
        0 // CORE 0
    );
}

void loop() {
    // Loop chính rảnh rỗi nhường CPU cho FreeRTOS
    vTaskDelay(pdMS_TO_TICKS(1000));
}