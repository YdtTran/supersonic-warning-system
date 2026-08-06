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
#include "EspNowConfig.h"
#include "EspNowClient.h"

static TaskHandle_t s_sensorTaskHandle = nullptr;
static TaskHandle_t s_appTaskHandle = nullptr;
static TaskHandle_t s_networkTaskHandle = nullptr;
static TaskHandle_t s_buzzerTaskHandle = nullptr;

static EspNowClient s_espNowClient;

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

                // Serial log tắt để tăng hiệu năng (tránh chặn task đo mỗi
                // chu kỳ). Bật lại khi cần debug.
                // float stableCm;
                // bool hasStable = s_filters[i].getStable(stableCm);
                // Serial.printf(
                //     "[S%u] REJECT: %s | Pulse: %lu us | Raw: %s | Stable: %s | Invalid: %d\n",
                //     (unsigned)i,
                //     reading.error,
                //     (unsigned long)reading.durationUs,
                //     distanceToText(reading.durationUs > 0, reading.distanceCm).c_str(),
                //     distanceToText(hasStable, stableCm).c_str(),
                //     s_invalidCount[i]);

                if (s_invalidCount[i] >= RESET_AFTER_INVALID)
                {
                    // Serial.printf("[S%u] FILTER RESET: mat tin hieu qua lau\n", (unsigned)i);
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

                // Serial log tắt để tăng hiệu năng (tránh chặn task đo mỗi
                // chu kỳ). Bật lại khi cần debug.
                // Serial.printf(
                //     "[S%u] RAW: %lu us | Raw: %s | Cluster: %s | Votes: %d | Spread: %s | Stable: %s | %s\n",
                //     (unsigned)i,
                //     (unsigned long)reading.durationUs,
                //     distanceToText(true, reading.distanceCm).c_str(),
                //     distanceToText(result.hasCluster, result.clusterCm).c_str(),
                //     result.clusterCount,
                //     distanceToText(result.hasSpread, result.clusterSpreadCm).c_str(),
                //     distanceToText(result.hasOutput, result.outputCm).c_str(),
                //     result.status);

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
// BUZZER TASK
// Đồng bộ với "relay"/"warning_status" phía CoreIoT (cùng ngưỡng
// khoảng cách): lấy khoảng cách gần nhất trong các cảm biến đã lọc,
// kêu 3s/lần khi WARNING (20-50cm), 1s/lần khi DANGER (<20cm), tắt
// hẳn khi không có vật cản trong tầm. Non-blocking, dùng millis() để
// không chặn task khác.
// =========================================================

static void buzzerTask(void *pvParameters)
{
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    uint32_t lastBeepStartMs = 0;
    bool beeping = false;

    for (;;)
    {
        float nearestCm = 0.0f;
        bool hasNearest = false;

        for (size_t i = 0; i < SENSOR_COUNT; ++i)
        {
            float distanceCm;
            if (sharedStateGet(i, distanceCm) && (!hasNearest || distanceCm < nearestCm))
            {
                nearestCm = distanceCm;
                hasNearest = true;
            }
        }

        uint32_t period = 0;
        if (hasNearest && nearestCm > 0.0f)
        {
            if (nearestCm < BUZZER_DANGER_DISTANCE_CM)
            {
                period = BUZZER_DANGER_PERIOD_MS;
            }
            else if (nearestCm <= BUZZER_WARNING_DISTANCE_CM)
            {
                period = BUZZER_WARNING_PERIOD_MS;
            }
        }

        uint32_t now = millis();

        if (period == 0)
        {
            if (beeping)
            {
                digitalWrite(BUZZER_PIN, LOW);
                beeping = false;
            }
        }
        else if (!beeping && (now - lastBeepStartMs >= period))
        {
            digitalWrite(BUZZER_PIN, HIGH);
            beeping = true;
            lastBeepStartMs = now;
        }
        else if (beeping && (now - lastBeepStartMs >= BUZZER_BEEP_ON_MS))
        {
            digitalWrite(BUZZER_PIN, LOW);
            beeping = false;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// =========================================================
// NETWORK TASK
// Gửi khoảng cách đã lọc trực tiếp tới waveshare-screen qua ESP-NOW mỗi
// ESPNOW_SEND_INTERVAL_MS - KHÔNG kết nối WiFi AP/MQTT tới CoreIoT trong
// nhánh này (tránh xung đột WiFi channel với ESP-NOW, xem
// docs/architecture/ESPNOW_NETWORK.md). Tách khỏi core 1 (sensorTask/
// appTask) để gửi ESP-NOW không ảnh hưởng timing đo cảm biến.
// =========================================================

static void networkTask(void *pvParameters)
{
    s_espNowClient.begin();

    uint32_t lastSendMs = 0;

    for (;;)
    {
        uint32_t now = millis();
        if (now - lastSendMs >= ESPNOW_SEND_INTERVAL_MS)
        {
            lastSendMs = now;

            // Message luôn mang đủ ESPNOW_SENSOR_SLOT_COUNT (6) vị trí
            // cảm biến để khớp mô hình sensor_model/ui_dashboard bên
            // waveshare-screen. Chỉ SENSOR_COUNT slot có phần cứng thật
            // trên board này (ánh xạ qua SENSOR_ESPNOW_SLOT) được set
            // valid=1; các slot còn lại giữ nguyên valid=0 ("null") -
            // waveshare-screen hiển thị "--" cho các slot đó.
            espnow_sensor_msg_t msg = {};

            for (size_t i = 0; i < SENSOR_COUNT; ++i)
            {
                float distanceCm;
                uint8_t slot = SENSOR_ESPNOW_SLOT[i];
                if (sharedStateGet(i, distanceCm))
                {
                    msg.distance_cm[slot] = distanceCm;
                    msg.valid[slot] = 1;
                }
            }

            s_espNowClient.sendReading(msg);
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
