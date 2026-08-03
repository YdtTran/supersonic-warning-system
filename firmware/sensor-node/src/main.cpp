// =========================================================
// Supersonic sensor array - Arduino IDE + FreeRTOS
// Đọc N cảm biến JSN-SR04T độc lập (mảng cặp chân Trig/Echo
// khai báo trong Config.h::SENSOR_PINS), mỗi cảm biến có
// UltrasonicSensor + DistanceFilter riêng, không dùng std::vector
// để giảm cấp phát động và tăng tốc xử lý trên vi điều khiển.
//
// Board: Yolo:Uno (ESP32-S3) - đã tích hợp sẵn FreeRTOS trong
// Arduino core, không cần cài thêm thư viện FreeRTOS.
//
// Ý tưởng non-blocking:
//   - Đo Echo bằng ngắt phần cứng (attachInterruptArg) thay vì
//     vòng lặp while() chờ pin đổi trạng thái.
//   - SensorTask đọc lần lượt từng cảm biến trong mảng (tránh
//     nhiễu âm học giữa các cảm biến khi trigger cùng lúc), chỉ
//     "ngủ" (xSemaphoreTake / vTaskDelay) trong lúc chờ.
//   - Kết quả khoảng cách của mỗi cảm biến được chia sẻ qua
//     SharedState (mảng, có mutex) để các task khác (vd AppTask)
//     dùng song song, không phụ thuộc chu kỳ đo của cảm biến.
// =========================================================

#include <Arduino.h>
#include "Config.h"
#include "UltrasonicSensor.h"
#include "DistanceFilter.h"
#include "SharedState.h"
#include "CoreiotConfig.h"
#include "CoreiotClient.h"

static TaskHandle_t s_sensorTaskHandle = nullptr;
static TaskHandle_t s_appTaskHandle = nullptr;
static TaskHandle_t s_networkTaskHandle = nullptr;

static CoreiotClient s_coreiotClient;

// Mảng tĩnh, kích thước cố định = SENSOR_COUNT (Config.h) - không dùng
// std::vector nên không có cấp phát heap/mảnh vụn bộ nhớ khi chạy.
static UltrasonicSensor s_sensors[SENSOR_COUNT];
static DistanceFilter s_filters[SENSOR_COUNT];
static int s_invalidCount[SENSOR_COUNT] = {0};

// =========================================================
// HÀM HỖ TRỢ IN
// =========================================================

static String distanceToText(bool valid, float cm)
{
    if (!valid)
    {
        return "--";
    }
    return String(cm, 2) + " cm";
}

// =========================================================
// SENSOR TASK
// Lần lượt trigger + đọc từng cảm biến trong mảng mỗi
// MEASURE_INTERVAL_MS, mỗi cảm biến chạy qua bộ lọc riêng.
// =========================================================

static void sensorTask(void *pvParameters)
{
    for (size_t i = 0; i < SENSOR_COUNT; ++i)
    {
        s_sensors[i].begin(SENSOR_PINS[i].trigPin, SENSOR_PINS[i].echoPin);
        s_filters[i].reset();
    }

    // Chờ cảm biến ổn định sau khi cấp nguồn (không block task khác)
    vTaskDelay(pdMS_TO_TICKS(500));

    Serial.println("========================================");
    Serial.printf("Supersonic sensor array started (%u cam bien)\n", (unsigned)SENSOR_COUNT);
    for (size_t i = 0; i < SENSOR_COUNT; ++i)
    {
        Serial.printf("  [S%u] Trig=GPIO%u Echo=GPIO%u\n",
                      (unsigned)i, SENSOR_PINS[i].trigPin, SENSOR_PINS[i].echoPin);
    }
    Serial.printf("Valid range: %.1f - %.1f cm\n", MIN_DISTANCE_CM, MAX_DISTANCE_CM);
    Serial.printf("Trigger pulse: %lu us\n", (unsigned long)TRIGGER_HIGH_US);
    Serial.printf("History: %d | Minimum cluster: %d\n", HISTORY_SIZE, MIN_CLUSTER_SIZE);
    Serial.printf("Interval: %lu ms\n", (unsigned long)MEASURE_INTERVAL_MS);
    Serial.println("========================================");

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        for (size_t i = 0; i < SENSOR_COUNT; ++i)
        {
            SensorReading reading = s_sensors[i].readOnce();

            if (reading.error != nullptr)
            {
                // -----------------------------------------------------
                // MẪU LỖI
                // -----------------------------------------------------
                s_invalidCount[i]++;

                float stableCm;
                bool hasStable = s_filters[i].getStable(stableCm);

                Serial.printf(
                    "[S%u] REJECT: %s | Pulse: %lu us | Raw: %s | Stable: %s | Invalid: %d\n",
                    (unsigned)i,
                    reading.error,
                    (unsigned long)reading.durationUs,
                    distanceToText(reading.durationUs > 0, reading.distanceCm).c_str(),
                    distanceToText(hasStable, stableCm).c_str(),
                    s_invalidCount[i]);

                if (s_invalidCount[i] >= RESET_AFTER_INVALID)
                {
                    Serial.printf("[S%u] FILTER RESET: mat tin hieu qua lau\n", (unsigned)i);
                    s_filters[i].reset();
                    sharedStateSet(i, 0.0f, false);
                    s_invalidCount[i] = 0;
                }
            }
            else
            {
                // -----------------------------------------------------
                // MẪU HỢP LỆ
                // -----------------------------------------------------
                s_invalidCount[i] = 0;

                FilterResult result = s_filters[i].process(reading.distanceCm);

                Serial.printf(
                    "[S%u] RAW: %lu us | Raw: %s | Cluster: %s | Votes: %d | Spread: %s | Stable: %s | %s\n",
                    (unsigned)i,
                    (unsigned long)reading.durationUs,
                    distanceToText(true, reading.distanceCm).c_str(),
                    distanceToText(result.hasCluster, result.clusterCm).c_str(),
                    result.clusterCount,
                    distanceToText(result.hasSpread, result.clusterSpreadCm).c_str(),
                    distanceToText(result.hasOutput, result.outputCm).c_str(),
                    result.status);

                sharedStateSet(i, result.outputCm, result.hasOutput);
            }
        }

        // vTaskDelayUntil giữ chu kỳ đo đều đặn, không cộng dồn độ trễ
        // và không chặn các task khác trong lúc chờ.
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(MEASURE_INTERVAL_MS));
    }
}

// =========================================================
// APP TASK (ví dụ)
// Minh họa task khác dùng khoảng cách đã lọc của TẤT CẢ cảm biến
// song song, hoàn toàn không bị chặn bởi thời gian đo.
// Thay phần này bằng logic điều khiển robot thực tế của bạn.
// =========================================================

static void appTask(void *pvParameters)
{
    pinMode(LED_BUILTIN, OUTPUT);

    for (;;)
    {
        bool anyClose = false;

        for (size_t i = 0; i < SENSOR_COUNT; ++i)
        {
            float distanceCm;
            if (sharedStateGet(i, distanceCm) && distanceCm < 50.0f)
            {
                anyClose = true;
                break;
            }
        }

        // Ví dụ: bất kỳ cảm biến nào thấy vật ở gần -> bật LED.
        // KHÔNG dùng raw_distance_cm để điều khiển robot,
        // chỉ dùng giá trị đã lọc (sharedStateGet).
        digitalWrite(LED_BUILTIN, anyClose ? HIGH : LOW);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// =========================================================
// NETWORK TASK
// Kết nối WiFi/MQTT tới CoreIoT và publish khoảng cách đã lọc
// của S3 (left_front, index 0) và S5 (right_front, index 1)
// mỗi COREIOT_PUBLISH_INTERVAL_MS. Tách khỏi core 1 (sensorTask/
// appTask) để publish MQTT không ảnh hưởng timing đo cảm biến.
// =========================================================

static void networkTask(void *pvParameters)
{
    s_coreiotClient.begin();

    uint32_t lastPublishMs = 0;

    for (;;)
    {
        s_coreiotClient.loop();

        uint32_t now = millis();
        if (now - lastPublishMs >= COREIOT_PUBLISH_INTERVAL_MS)
        {
            lastPublishMs = now;

            float leftFrontCm, rightFrontCm;
            bool hasLeftFront = sharedStateGet(0, leftFrontCm);
            bool hasRightFront = sharedStateGet(1, rightFrontCm);

            if (hasLeftFront || hasRightFront)
            {
                char payload[96];
                if (hasLeftFront && hasRightFront)
                {
                    snprintf(payload, sizeof(payload),
                             "{\"left_front\":%.1f,\"right_front\":%.1f}",
                             leftFrontCm, rightFrontCm);
                }
                else if (hasLeftFront)
                {
                    snprintf(payload, sizeof(payload), "{\"left_front\":%.1f}", leftFrontCm);
                }
                else
                {
                    snprintf(payload, sizeof(payload), "{\"right_front\":%.1f}", rightFrontCm);
                }

                bool ok = s_coreiotClient.publishTelemetry(payload);
                Serial.printf("[NET] Publish %s: %s\n", ok ? "OK" : "FAILED", payload);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// =========================================================
// SETUP / LOOP
// =========================================================

void setup()
{
    Serial.begin(115200);
    delay(200);

    sharedStateInit();

    // Task đọc/lọc cảm biến - ưu tiên cao hơn vì có ràng buộc thời gian
    // (timeout Echo tính bằng us).
    xTaskCreatePinnedToCore(
        sensorTask,
        "SensorTask",
        4096,
        nullptr,
        2,
        &s_sensorTaskHandle,
        1);

    // Task ứng dụng ví dụ - chạy song song, độc lập với chu kỳ đo.
    xTaskCreatePinnedToCore(
        appTask,
        "AppTask",
        2048,
        nullptr,
        1,
        &s_appTaskHandle,
        1);

    // Task mạng - chạy trên core 0, tách khỏi core 1 (đo/lọc cảm biến)
    // để publish MQTT không ảnh hưởng timing đo.
    xTaskCreatePinnedToCore(
        networkTask,
        "NetworkTask",
        4096,
        nullptr,
        1,
        &s_networkTaskHandle,
        0);
}

void loop()
{
    // Toàn bộ xử lý đã chuyển vào FreeRTOS task ở trên,
    // nên không cần dùng loop() nữa.
    vTaskDelete(nullptr);
}
