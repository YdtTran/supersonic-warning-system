#include <Arduino.h>

#define NUM_SENSORS 1
const int SENSOR_RX_PIN = 43;

// Sử dụng Hardware UART 1 của ESP32-S3
HardwareSerial SensorSerial(1);

// Cấu trúc dữ liệu chứa kết quả đo của cảm biến
typedef struct
{
    uint16_t distances_mm[NUM_SENSORS];
    bool valid[NUM_SENSORS];
    uint32_t timestamp;
} SensorBatch_t;

// Queue giao tiếp giữa Core 1 (Đọc sensor) và Core 0 (Hiển thị / Ứng dụng)
QueueHandle_t xSensorQueue = NULL;

// Handle Task
TaskHandle_t TaskSensorHandle = NULL;
TaskHandle_t TaskDisplayHandle = NULL;

// Hàm đọc 1 gói tin 4-byte từ Hardware UART
bool readHardwareUartSensorPacket(HardwareSerial &serial, uint16_t *distance_mm, uint32_t timeoutMs)
{
    uint32_t startMs = millis();
    while (millis() - startMs < timeoutMs)
    {
        if (serial.available() >= 4)
        {
            if (serial.peek() == 0xFF)
            {
                uint8_t buf[4];
                serial.readBytes(buf, 4);
                uint8_t calcChecksum = (buf[0] + buf[1] + buf[2]) & 0xFF;
                if (calcChecksum == buf[3])
                {
                    *distance_mm = (buf[1] << 8) | buf[2];
                    return true;
                }
            }
            else
            {
                serial.read(); // Loại bỏ byte rác để đồng bộ lại khung truyền
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return false;
}

// --- TASK 1: ĐỌC CẢM BIẾN BẰNG HARDWARE UART (CỐ ĐỊNH TRÊN CORE 1) ---
void TaskReadSensorHardwareUART(void *pvParameters)
{
    Serial.printf("[RTOS] TaskReadSensorHardwareUART đang chạy trên CORE %d (Hardware UART1, RX GPIO %d)\n",
                  xPortGetCoreID(), SENSOR_RX_PIN);

    // Khởi tạo Hardware UART 1: Baudrate 9600, RX=GPIO 44, TX=-1
    SensorSerial.begin(9600, SERIAL_8N1, SENSOR_RX_PIN, -1);

    SensorBatch_t batch;

    for (;;)
    {
        batch.timestamp = millis();

        uint16_t dist_mm = 0;
        bool ok = readHardwareUartSensorPacket(SensorSerial, &dist_mm, 50);

        batch.distances_mm[0] = ok ? dist_mm : 0;
        batch.valid[0] = ok;

        // Đẩy kết quả vào FreeRTOS Queue (Non-blocking)
        if (xSensorQueue != NULL)
        {
            xQueueSend(xSensorQueue, &batch, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- TASK 2: HIỂN THỊ DỮ LIỆU NON-BLOCKING (CỐ ĐỊNH TRÊN CORE 0) ---
void TaskDisplayMain(void *pvParameters)
{
    Serial.printf("[RTOS] TaskDisplayMain đang chạy trên CORE %d\n", xPortGetCoreID());

    SensorBatch_t receivedBatch;

    for (;;)
    {
        if (xQueueReceive(xSensorQueue, &receivedBatch, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            Serial.printf("\n--- [TIME: %6d ms] KẾT QUẢ ĐỌC HARDWARE UART (GPIO %d) ---\n",
                          receivedBatch.timestamp, SENSOR_RX_PIN);

            if (receivedBatch.valid[0])
            {
                float cm = receivedBatch.distances_mm[0] / 10.0f;
                Serial.printf("  Sensor 1 (GPIO %2d): %4d mm (%5.1f cm) [OK]\n",
                              SENSOR_RX_PIN, receivedBatch.distances_mm[0], cm);
            }
            else
            {
                Serial.printf("  Sensor 1 (GPIO %2d): TIMEOUT / CHECKSUM ERROR [FAIL]\n",
                              SENSOR_RX_PIN);
            }
            Serial.println("--------------------------------------------------");
        }
    }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 2000)
        ;

    Serial.println("\n=======================================================");
    Serial.println("  HỆ THỐNG CẢM BIẾN SIÊU ÂM - HARDWARE UART DUAL CORE");
    Serial.println("  Hardware UART1 RX Pin: GPIO 44                        ");
    Serial.println("  Core 1: TaskReadSensorHardwareUART                     ");
    Serial.println("  Core 0: TaskDisplayMain (FreeRTOS Queue)               ");
    Serial.println("=======================================================\n");

    xSensorQueue = xQueueCreate(5, sizeof(SensorBatch_t));

    if (xSensorQueue == NULL)
    {
        Serial.println("[LỖI] Không thể tạo FreeRTOS Queue!");
        return;
    }

    xTaskCreatePinnedToCore(
        TaskReadSensorHardwareUART,
        "TaskReadSensorHardwareUART",
        4096,
        NULL,
        5,
        &TaskSensorHandle,
        1 // CORE 1
    );

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

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}