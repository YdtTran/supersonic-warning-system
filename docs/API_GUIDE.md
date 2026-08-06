# Hướng dẫn API & Cấu hình phần mềm

> Tài liệu tham khảo cho lập trình viên: API của các thư viện/component tự viết trong dự án, kèm ví dụ sử dụng và cách cấu hình bằng phần mềm (không cần sửa phần cứng). Tổng quan kiến trúc/lý do thiết kế xem [`report/README.md`](../report/README.md); mô tả từng module xem README riêng của [`firmware/sensor-node`](../firmware/sensor-node/README.md) và [`firmware/waveshare-screen`](../firmware/waveshare-screen/README.md).
>
> **Nhánh hiện tại dùng ESP-NOW trực tiếp giữa 2 board, không qua CoreIoT/MQTT** — các mục **1.4, 2.2, 3.1 và toàn bộ mục 4** dưới đây mô tả đường CoreIoT/Rule-Chain **không hoạt động** trên nhánh này (code vẫn còn trong cây, giữ lại để khôi phục sau). Xem mục **1.5** và **2.2b** cho API ESP-NOW đang dùng thật, và [`docs/architecture/ESPNOW_NETWORK.md`](architecture/ESPNOW_NETWORK.md)/[`docs/architecture/DATA_SCHEMA.md`](architecture/DATA_SCHEMA.md) cho schema.

## Mục lục

1. [`firmware/sensor-node` (Arduino) — thư viện đo & lọc cảm biến](#1-firmwaresensor-node-arduino--thư-viện-đo--lọc-cảm-biến)
2. [`firmware/waveshare-screen` (ESP-IDF) — component dashboard](#2-firmwarewaveshare-screen-esp-idf--component-dashboard)
3. [Cấu hình bằng phần mềm — không cần sửa code logic](#3-cấu-hình-bằng-phần-mềm--không-cần-sửa-code-logic)
4. [Rule-Chain CoreIoT — cấu hình & API node — không dùng trên nhánh hiện tại](#4-rule-chain-coreiot--cấu-hình--api-node)
5. [Lưu ý bảo mật](#5-lưu-ý-bảo-mật)

---

## 1. `firmware/sensor-node` (Arduino) — thư viện đo & lọc cảm biến

Framework: `arduino` (PlatformIO, board `yolo_uno`). 4 thư viện tự viết, mỗi thư viện có 1 trách nhiệm duy nhất, không dùng `std::vector`/cấp phát động để tránh phân mảnh heap trên vi điều khiển.

### 1.1 `UltrasonicSensor` — đọc 1 cảm biến JSN-SR04T V3

Header: [`include/UltrasonicSensor.h`](../firmware/sensor-node/include/UltrasonicSensor.h)

Cảm biến vật lý trên `sensor-node` là **JSN-SR04T V3**, chạy ở **Mode 0 (mặc định)**: MCU phát xung Trig, đọc trực tiếp độ rộng xung Echo qua GPIO — không qua UART, không cần chỉnh jumper `R27` trên board.

| API | Mô tả |
|---|---|
| `void begin(uint8_t trigPin, uint8_t echoPin)` | Khởi tạo chân Trig/Echo + ngắt ngoài + semaphore. Gọi 1 lần trước khi dùng `readOnce()`. |
| `SensorReading readOnce()` | Phát xung Trig, đo độ rộng xung Echo qua ngắt phần cứng (không busy-wait). Chỉ gọi từ **một** task cho **một** instance tại một thời điểm. |

`SensorReading` trả về gồm `durationUs` (độ rộng xung Echo), `distanceCm` (chỉ có ý nghĩa khi `error == nullptr`), `error` (`nullptr` nếu hợp lệ, ngược lại là lý do reject: vượt phạm vi, timeout Echo, v.v.).

Mỗi cảm biến cần **1 instance riêng** (mảng tĩnh `UltrasonicSensor sensors[SENSOR_COUNT]`), vì trạng thái ISR (`_echoRiseUs`, `_echoDurationUs`) không dùng chung.

```cpp
#include "UltrasonicSensor.h"
#include "Config.h"

UltrasonicSensor sensors[SENSOR_COUNT];

void setup() {
    for (size_t i = 0; i < SENSOR_COUNT; ++i) {
        sensors[i].begin(SENSOR_PINS[i].trigPin, SENSOR_PINS[i].echoPin);
    }
}

void loop() {
    for (size_t i = 0; i < SENSOR_COUNT; ++i) {
        SensorReading r = sensors[i].readOnce();
        if (r.error == nullptr) {
            Serial.printf("[S%u] %.1f cm\n", (unsigned)i, r.distanceCm);
        } else {
            Serial.printf("[S%u] reject: %s\n", (unsigned)i, r.error);
        }
    }
    delay(100); // >= MEASURE_INTERVAL_MS để tránh nhiễu âm học giữa các cảm biến
}
```

### 1.2 `DistanceFilter` — lọc cụm (cluster) + EMA

Header: [`include/DistanceFilter.h`](../firmware/sensor-node/include/DistanceFilter.h)

| API | Mô tả |
|---|---|
| `void reset()` | Xoá toàn bộ trạng thái (lịch sử mẫu, kết quả ổn định, ứng viên bước nhảy). Gọi khi mất tín hiệu lâu (`RESET_AFTER_INVALID`). |
| `FilterResult process(float rawDistanceCm)` | Đưa 1 mẫu RAW đã hợp lệ (qua kiểm tra phạm vi ở `UltrasonicSensor`) vào bộ lọc. |
| `bool getStable(float &outCm) const` | Lấy khoảng cách ổn định gần nhất; trả `false` nếu chưa có. |

`FilterResult.status` là một trong `"WARMUP" | "NO_CLUSTER" | "INIT" | "OK" | "HOLD_JUMP" | "ACCEPT_JUMP"` — dùng để debug/log, **không** dùng để quyết định điều khiển (dùng `hasOutput`/`outputCm`).

Mỗi cảm biến cần **1 instance riêng** (cùng mảng index với `UltrasonicSensor`):

```cpp
#include "DistanceFilter.h"

DistanceFilter filters[SENSOR_COUNT];

void setup() {
    for (size_t i = 0; i < SENSOR_COUNT; ++i) filters[i].reset();
}

// Trong vòng đo, sau khi có SensorReading r hợp lệ (r.error == nullptr):
FilterResult result = filters[i].process(r.distanceCm);
if (result.hasOutput) {
    // Dùng result.outputCm cho logic điều khiển/cảnh báo,
    // KHÔNG dùng r.distanceCm (raw) trực tiếp.
}
```

### 1.3 `SharedState` — chia sẻ dữ liệu giữa các task FreeRTOS

Header: [`include/SharedState.h`](../firmware/sensor-node/include/SharedState.h)

| API | Mô tả |
|---|---|
| `void sharedStateInit()` | Khởi tạo mutex. Gọi 1 lần trong `setup()` trước mọi task. |
| `void sharedStateSet(size_t sensorIndex, float distanceCm, bool valid)` | Ghi khoảng cách đã lọc của cảm biến `sensorIndex`. Gọi từ `SensorTask`. |
| `bool sharedStateGet(size_t sensorIndex, float &distanceCm)` | Đọc khoảng cách ổn định gần nhất; `true` nếu hợp lệ. Gọi từ bất kỳ task nào khác (`AppTask`, `NetworkTask`, `buzzerTask`). |

```cpp
#include "SharedState.h"

// Task khác đọc song song, độc lập chu kỳ đo (xem appTask trong main.cpp):
float distanceCm;
if (sharedStateGet(/*sensorIndex=*/0, distanceCm) && distanceCm < 50.0f) {
    // vật cản gần trong tầm cảm biến 0 (S3, left_front)
}
```

### 1.4 `CoreiotClient` — publish MQTT lên CoreIoT (ThingsBoard) — **không dùng trên nhánh hiện tại**

Header: [`include/CoreiotClient.h`](../firmware/sensor-node/include/CoreiotClient.h)

| API | Mô tả |
|---|---|
| `void begin()` | Khởi tạo WiFi STA + cấu hình MQTT client. Gọi 1 lần trong `networkTask`. |
| `void loop()` | Gọi liên tục trong vòng lặp `networkTask`: giữ WiFi/MQTT sống, xử lý `PubSubClient::loop()`. Không dùng `delay()` nên không chặn task. |
| `bool isConnected() const` | Trạng thái kết nối MQTT hiện tại. |
| `bool publishTelemetry(const char *jsonPayload)` | Gửi JSON lên `COREIOT_TELEMETRY_TOPIC`. Trả `false` nếu chưa kết nối hoặc publish lỗi. |

```cpp
#include "CoreiotClient.h"
#include "CoreiotConfig.h"

CoreiotClient client;

void networkTask(void *) {
    client.begin();
    for (;;) {
        client.loop();
        if (client.isConnected()) {
            char payload[96];
            snprintf(payload, sizeof(payload), "{\"left_front\":%.1f}", 123.4f);
            client.publishTelemetry(payload);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

### 1.5 `EspNowClient` — gửi trực tiếp tới `waveshare-screen` qua ESP-NOW (đang dùng)

Header: [`include/EspNowClient.h`](../firmware/sensor-node/include/EspNowClient.h), cấu hình: [`include/EspNowConfig.h`](../firmware/sensor-node/include/EspNowConfig.h). Schema đầy đủ: [`docs/architecture/ESPNOW_NETWORK.md`](architecture/ESPNOW_NETWORK.md).

| API | Mô tả |
|---|---|
| `void begin()` | `WiFi.mode(WIFI_STA)` + set channel cố định (`ESPNOW_CHANNEL`, không `WiFi.begin()`), khởi tạo `esp_now`, đăng ký peer `ESPNOW_PEER_MAC`. Gọi 1 lần trong `networkTask`. |
| `bool sendReading(const espnow_sensor_msg_t &msg)` | `esp_now_send()` struct 6-slot tới `ESPNOW_PEER_MAC`. Trả `false` nếu `esp_now_send()` lỗi (không đảm bảo đã tới nơi — xem callback `onDataSent` trong `.cpp` để log kết quả gửi thật). |

```cpp
#include "EspNowClient.h"
#include "EspNowConfig.h"

EspNowClient client;

void networkTask(void *) {
    client.begin();
    for (;;) {
        espnow_sensor_msg_t msg = {}; // valid[] mặc định 0 = "null" cho slot chưa lắp phần cứng
        float distanceCm;
        if (sharedStateGet(/*sensorIndex=*/0, distanceCm)) {
            uint8_t slot = SENSOR_ESPNOW_SLOT[0];
            msg.distance_cm[slot] = distanceCm;
            msg.valid[slot] = 1;
        }
        client.sendReading(msg);
        vTaskDelay(pdMS_TO_TICKS(ESPNOW_SEND_INTERVAL_MS));
    }
}
```

---

## 2. `firmware/waveshare-screen` (ESP-IDF) — component dashboard

Framework: `espidf` thuần (không Arduino — lý do xem [`report/README.md` mục 9.4](../report/README.md#94-quyết-định-kiến-trúc-arduino-hybrid-thất-bại--esp-idf-thuần)). 3 component ESP-IDF, mỗi component có `CMakeLists.txt` + `include/*.h` riêng, độc lập nhau (`sensor_model` không phụ thuộc `coreiot_client`/`ui_dashboard`).

### 2.1 `sensor_model` — mô hình dữ liệu 6 cảm biến (thread-safe)

Header: [`components/sensor_model/include/sensor_model.h`](../firmware/waveshare-screen/components/sensor_model/include/sensor_model.h)

| API | Mô tả |
|---|---|
| `void sensor_model_init(void)` | Khởi tạo mutex + giá trị mặc định. Gọi 1 lần trong `app_main()`. |
| `void sensor_model_set_distance(sensor_id_t id, uint16_t distance_cm)` | Cập nhật khoảng cách 1 cảm biến (thread-safe). |
| `void sensor_model_clear(sensor_id_t id)` | Đánh dấu 1 cảm biến "không có dữ liệu" (`distance_cm=0`, `is_stale=true`) — dùng khi ESP-NOW báo `valid=0` cho slot đó, để không hiển thị/dùng distance cũ. Thread-safe. |
| `sensor_reading_t sensor_model_get(sensor_id_t id)` | Đọc snapshot 1 cảm biến (thread-safe). |
| `void sensor_model_get_all(sensor_reading_t out[SENSOR_MODEL_COUNT])` | Đọc snapshot cả 6 cảm biến cùng lúc. |
| `sensor_zone_t sensor_model_classify(uint16_t distance_cm)` | Phân loại `SENSOR_ZONE_SAFE` (>100cm) / `CAUTION` (30–100cm) / `DANGER` (<30cm). |

`sensor_id_t` gồm `SENSOR_ID_FRONT/REAR/LEFT_FRONT/LEFT_REAR/RIGHT_FRONT/RIGHT_REAR` (0–5, khớp thứ tự `S1..S6`).

> Schema đầy đủ của struct này (bảng field, và lưu ý chỉ 3/6 slot có dữ liệu sống vì mới lắp 3 cảm biến phần cứng): [`docs/architecture/DATA_SCHEMA.md` mục 3](architecture/DATA_SCHEMA.md#3-sensor_model--struct-nội-bộ-trên-waveshare-screen).

```c
#include "sensor_model.h"

void app_main(void) {
    sensor_model_init();
    // Khi nhận message ESP-NOW (xem mục 2.2b bên dưới):
    sensor_model_set_distance(SENSOR_ID_LEFT_FRONT, 85);   // valid=1
    sensor_model_clear(SENSOR_ID_REAR);                     // valid=0 cho slot đó

    sensor_reading_t r = sensor_model_get(SENSOR_ID_LEFT_FRONT);
    sensor_zone_t zone = sensor_model_classify(r.distance_cm);
    if (zone == SENSOR_ZONE_DANGER) {
        // ...
    }
}
```

### 2.2 `coreiot_client` — Wi-Fi STA + MQTT (ESP-IDF thuần, callback-based) — **không dùng trên nhánh hiện tại**

Header: [`components/coreiot_client/include/coreiot_client.h`](../firmware/waveshare-screen/components/coreiot_client/include/coreiot_client.h)

| API | Mô tả |
|---|---|
| `void coreiot_client_set_callbacks(wifi_cb, mqtt_cb, data_cb)` | Đăng ký callback **trước khi** gọi `coreiot_client_init()`. Tham số nào không cần thì truyền `NULL`. |
| `void coreiot_client_init(void)` | Khởi tạo NVS, Wi-Fi STA, MQTT client. Gọi 1 lần, thường trong 1 task riêng (`networkTask`, core 0). |
| `int coreiot_client_publish_telemetry(const char *json_payload)` | Publish JSON lên `COREIOT_MQTT_TELEMETRY_TOPIC`. Trả về `msg_id`, hoặc `-1` nếu lỗi. |
| `bool coreiot_client_has_recent_data(uint32_t max_age_ms)` | Kiểm tra có nhận được dữ liệu MQTT trong `max_age_ms` gần nhất không (dùng để hiện badge "mất kết nối"). |

3 kiểu callback:
- `coreiot_wifi_status_cb_t(bool is_connected, const char *ip_str)`
- `coreiot_mqtt_status_cb_t(bool is_connected)`
- `coreiot_data_cb_t(const char *topic, int topic_len, const char *payload, int payload_len)` — **payload không null-terminated**, phải dùng kèm `payload_len` khi parse JSON (`cJSON_ParseWithLength` hoặc copy ra buffer trước).

```c
#include "coreiot_client.h"
#include "sensor_model.h"
#include "cJSON.h"

static void on_wifi_status(bool connected, const char *ip) {
    // vd: ui_dashboard_set_iot_status(connected, ip);
}

static void on_mqtt_data(const char *topic, int topic_len, const char *payload, int payload_len) {
    cJSON *root = cJSON_ParseWithLength(payload, payload_len);
    if (!root) return;
    cJSON *lf = cJSON_GetObjectItem(root, "left_front");
    if (cJSON_IsNumber(lf)) {
        sensor_model_set_distance(SENSOR_ID_LEFT_FRONT, (uint16_t)lf->valuedouble);
    }
    cJSON_Delete(root);
}

void app_main(void) {
    coreiot_client_set_callbacks(on_wifi_status, NULL, on_mqtt_data);
    coreiot_client_init(); // hoặc gọi trong 1 task riêng, xem firmware/waveshare-screen/src/main.c
}
```

### 2.2b Nhận ESP-NOW trực tiếp (`src/main.c`, đang dùng)

Không phải 1 component riêng — logic nhận nằm thẳng trong [`src/main.c`](../firmware/waveshare-screen/src/main.c) vì `esp_now` là API hệ thống ESP-IDF, không cần wrapper. `WiFi.mode(WIFI_STA)` không gọi `esp_wifi_connect()`, chỉ set channel cố định (khớp `ESPNOW_CHANNEL` bên `sensor-node`) rồi đăng ký callback `esp_now_register_recv_cb(on_data_recv)`.

```c
static void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len != sizeof(espnow_sensor_msg_t)) return;
    espnow_sensor_msg_t msg;
    memcpy(&msg, data, sizeof(msg));

    if (esp_lv_adapter_lock(100)) {
        for (int i = 0; i < ESPNOW_SENSOR_SLOT_COUNT; i++) {
            if (msg.valid[i]) {
                ui_dashboard_update_sensor((uint8_t)i, (uint16_t)msg.distance_cm[i]);
            } else {
                ui_dashboard_clear_sensor((uint8_t)i); // valid=0 = "null", không giữ distance cũ
            }
        }
        esp_lv_adapter_unlock();
    }
}
```

Một `esp_timer` 1s kiểm tra thời điểm nhận gần nhất, gọi `ui_dashboard_set_espnow_status(false)` (badge "ESP-NOW: NO LINK") nếu quá 1.5s không nhận được message nào — thay cho badge Wi-Fi/MQTT của đường CoreIoT cũ.

### 2.3 `ui_dashboard` — giao diện LVGL 9.1 (800×480)

Header: [`components/ui_dashboard/include/ui_dashboard.h`](../firmware/waveshare-screen/components/ui_dashboard/include/ui_dashboard.h)

**Toàn bộ hàm `ui_dashboard_*` phải được gọi trong `esp_lv_adapter_lock()`/`unlock()`** — LVGL không thread-safe. Callback Wi-Fi/MQTT (chạy trên task khác task LVGL) phải dùng timeout hữu hạn (`esp_lv_adapter_lock(100)`, không dùng `-1`) để tránh deadlock nếu task LVGL bị kẹt (xem [`report/README.md` mục 6.3](../report/README.md#63-ui_dashboard--giao-diện-lvgl-91)).

| API | Mô tả |
|---|---|
| `void ui_dashboard_init(void)` | Dựng toàn bộ layout (header, tab COLLISION/SYSTEM, sidebar, canvas xe 2D + 6 arc cảm biến). Gọi 1 lần lúc khởi tạo. |
| `void ui_dashboard_update_sensor(uint8_t sensor_id, uint16_t dist_cm)` | Cập nhật 1 dòng sidebar + màu/nhấp nháy arc + re-evaluate hazard. `sensor_id`: 0=S1(Front) 1=S2(Rear) 2=S3(Left-Front) 3=S4(Left-Rear) 4=S5(Right-Front) 5=S6(Right-Rear). |
| `void ui_dashboard_clear_sensor(uint8_t sensor_id)` | Đánh dấu 1 trong 6 cảm biến "không có dữ liệu" (ESP-NOW báo `valid=0` cho slot đó) — sidebar về "-- cm", arc đổi màu xám trung tính, re-evaluate hazard (loại slot này khỏi tính toán). |
| `void ui_dashboard_set_iot_status(bool is_connected, const char *ip)` | *(không dùng trên nhánh hiện tại)* Cập nhật badge Wi-Fi/MQTT ở header + tab SYSTEM — đường CoreIoT. |
| `void ui_dashboard_set_espnow_status(bool linked)` | Cập nhật badge header "ESP-NOW: LINKED"/"ESP-NOW: NO LINK" — gọi từ watchdog `esp_timer` trong `main.c` (xem mục 2.2b). |
| `void ui_dashboard_set_hazard_warning(bool is_pedestrian_crossing_risk)` | Ép hiện banner "CROSSING TRAFFIC HAZARD", độc lập với logic tự động. |
| `void ui_dashboard_set_relay_state(bool relay_on, const char *warning_status)` | *(không dùng trên nhánh hiện tại)* Hiện trạng thái `relay`/`warning_status` do Rule-Chain CoreIoT tính — nhánh hiện tại dùng banner "OVERALL: ..." tự đánh giá cục bộ (`evaluate_hazard()`) thay cho hàm này. |
| `void ui_dashboard_set_buzzer_state(bool buzzer_on)` | Hiện dòng `BUZZER: ON/OFF`, phản ánh buzzer vật lý điều khiển cục bộ trên `sensor-node`. |

```c
#include "ui_dashboard.h"
#include "esp_lv_adapter.h" // esp_lv_adapter_lock/unlock

void update_from_espnow(uint8_t sensor_id, uint16_t dist_cm, bool valid) {
    if (esp_lv_adapter_lock(100)) {   // timeout 100ms, KHÔNG dùng -1 trong callback network
        if (valid) {
            ui_dashboard_update_sensor(sensor_id, dist_cm);
        } else {
            ui_dashboard_clear_sensor(sensor_id);
        }
        esp_lv_adapter_unlock();
    }
}
```

---

## 3. Cấu hình bằng phần mềm — không cần sửa code logic

Tất cả các mục dưới đây chỉ cần sửa **giá trị hằng số** trong file header, không đụng vào logic xử lý.

### 3.1 Đổi Wi-Fi / MQTT CoreIoT — **không dùng trên nhánh hiện tại**

| Firmware | File | Hằng số |
|---|---|---|
| `sensor-node` | [`include/CoreiotConfig.h`](../firmware/sensor-node/include/CoreiotConfig.h) | `COREIOT_WIFI_SSID`, `COREIOT_WIFI_PASS`, `COREIOT_MQTT_HOST`, `COREIOT_MQTT_PORT`, `COREIOT_MQTT_TOKEN`, `COREIOT_MQTT_CLIENT_ID` |
| `waveshare-screen` | [`components/coreiot_client/include/coreiot_client.h`](../firmware/waveshare-screen/components/coreiot_client/include/coreiot_client.h) | `COREIOT_WIFI_SSID`, `COREIOT_WIFI_PASS`, `COREIOT_MQTT_BROKER_URI`, `COREIOT_MQTT_ACCESS_TOKEN`, `COREIOT_DEVICE_ID` |

Nếu cấp lại Access Token trên CoreIoT, phải đổi **cả 2 nơi trên** cộng thêm `SENSOR_NODE_DEVICE_TOKEN` trong `config/keys.json` (dùng bởi `tools/test_mqtt_coreiot.py`), nếu không 2 firmware sẽ mất đồng bộ dữ liệu.

### 3.2 Thêm/bớt cảm biến trên `sensor-node`

Sửa `SENSOR_PINS[]` và `SENSOR_COUNT` trong [`include/Config.h`](../firmware/sensor-node/include/Config.h):

```cpp
static const SensorPinConfig SENSOR_PINS[] = {
    {5, 6},   // Cảm biến 0 (Trig=GPIO5, Echo=GPIO6)
    {7, 8},   // Cảm biến 1
    {9, 10},  // Cảm biến 2
    {15, 16}, // Cảm biến mới — thêm 1 dòng {trig, echo}
};
static const size_t SENSOR_COUNT = 4;
```

`main.cpp` (`sensorTask`, `s_sensors[]`, `s_filters[]`) đọc trực tiếp `SENSOR_COUNT` nên không cần sửa thêm. Để cảm biến mới xuất hiện trên `waveshare-screen`, phải thêm 1 dòng tương ứng vào `SENSOR_ESPNOW_SLOT[SENSOR_COUNT]` trong [`include/EspNowConfig.h`](../firmware/sensor-node/include/EspNowConfig.h) — chọn slot ESP-NOW (`ESPNOW_SLOT_FRONT/REAR/LEFT_FRONT/LEFT_REAR/RIGHT_FRONT/RIGHT_REAR`) khớp vị trí lắp thật của cảm biến, đúng thứ tự với `SENSOR_PINS[]` (xem [`docs/architecture/ESPNOW_NETWORK.md`](architecture/ESPNOW_NETWORK.md)). Không cần sửa gì phía `waveshare-screen` — cả 6 slot đã được xử lý sẵn trong `on_data_recv()`.

### 3.3 Tinh chỉnh bộ lọc khoảng cách (Cluster + EMA)

Toàn bộ tham số nằm trong [`include/Config.h`](../firmware/sensor-node/include/Config.h) (mô tả chi tiết ý nghĩa từng tham số ở [`report/README.md` mục 5.2](../report/README.md#52-bộ-lọc-khoảng-cách-cluster--ema)):

| Muốn | Sửa |
|---|---|
| Kết quả mượt hơn nhưng phản ứng chậm hơn | Giảm `EMA_ALPHA` (vd 0.30 → 0.15) |
| Phản ứng nhanh hơn nhưng dễ nhiễu hơn | Tăng `EMA_ALPHA` (vd 0.30 → 0.5) |
| Xác nhận vật cản mới nhanh hơn (đổi lấy nguy cơ báo giả) | Giảm `JUMP_CONFIRM_COUNT` |
| Cảm biến lắp gần vật phản xạ nhiễu (tường/kính) | Tăng `BASE_CLUSTER_TOLERANCE_CM` / `MIN_CLUSTER_SIZE` |

### 3.4 Đổi ngưỡng cảnh báo WARNING/DANGER (buzzer + UI)

Trên nhánh hiện tại (không còn Rule-Chain CoreIoT nhận dữ liệu), buzzer vật lý và banner "OVERALL" trên UI là **2 ngưỡng độc lập, không cần đồng bộ với nhau**:

1. Buzzer (`sensor-node`): `BUZZER_WARNING_DISTANCE_CM`, `BUZZER_DANGER_DISTANCE_CM`, `BUZZER_WARNING_PERIOD_MS`, `BUZZER_DANGER_PERIOD_MS` trong [`include/Config.h`](../firmware/sensor-node/include/Config.h).
2. Zone hiển thị (`waveshare-screen`): ngưỡng SAFE/CAUTION/DANGER trong `sensor_model_classify()` ([`components/sensor_model/sensor_model.c`](../firmware/waveshare-screen/components/sensor_model/sensor_model.c)) — hiện là mã nguồn cứng (>100/30–100/<30 cm), muốn đổi phải sửa file `.c` (không phải tham số cấu hình thuần). Banner "OVERALL" (`evaluate_hazard()` trong `ui_dashboard.c`) dùng trực tiếp zone tệ nhất trong 6 cảm biến, không có ngưỡng riêng.

> Đường CoreIoT Rule-Chain (mục 4) vẫn giữ ngưỡng `< 20cm`/`<= 50cm` riêng trong `jsScript`, nhưng **không nhận được dữ liệu** trên nhánh này nên sửa ở đó không ảnh hưởng gì tới thiết bị thật cho tới khi đường MQTT được khôi phục.

### 3.5 Chọn board / cổng nạp (PlatformIO)

Cả 2 firmware dùng chung layout `platformio.ini` + `boards/yolo_uno.json`:

```bash
cd firmware/sensor-node        # hoặc firmware/waveshare-screen
pio run -e yolo_uno                    # build
pio run -e yolo_uno -t upload          # flash (tự dò cổng COM)
pio run -e yolo_uno -t upload --upload-port COM9   # ép cổng cụ thể
pio device monitor -b 115200 -p COM9   # xem log Serial
```

`waveshare-screen` có thêm script tiện lợi [`build_and_flash.bat`](../firmware/waveshare-screen/build_and_flash.bat) (mặc định `COM9`, xem [README](../firmware/waveshare-screen/README.md#-hướng-dẫn-biên-dịch--nạp-firmware-build--flash)).

---

## 4. Rule-Chain CoreIoT — cấu hình & API node

> **Không dùng trên nhánh hiện tại.** Toàn bộ mục này mô tả đường CoreIoT/Rule-Chain của kiến trúc trước ESP-NOW — `waveshare-screen` không còn nhận MQTT nên rule-chain này hiện không có tác dụng gì với thiết bị thật, dù vẫn export/import được trên CoreIoT. Giữ lại làm tài liệu tham khảo nếu khôi phục đường MQTT sau này.

File cấu hình: [`cloud/coreiot/rule_chain/supersonic_rule_chain.json`](../cloud/coreiot/rule_chain/supersonic_rule_chain.json) — export/import trực tiếp trên UI CoreIoT (**Rule Chains → (chọn chain) → ⋮ → Export/Import**), không có API lập trình riêng (cấu hình thuần bằng JSON + UI kéo-thả).

### 4.1 Sơ đồ node & luồng xử lý

```text
                         ┌─► [0] Save Timeseries (sensor-node)         "Post telemetry"
                         │
[2] Message Type Switch ─┼─► [1] Save Attributes                      "Post attributes"
                         │
                         └─► [3] Process Ultrasonic & Vehicle Data     "Post telemetry"
                                       │ Success
                                       ▼
                             [4] Change Originator → waveshare-screen
                                       │ Success                 │ Success
                                       ▼                          ▼
                             [5] Update Shared Attributes   [6] Save Timeseries
                                 (SHARED_SCOPE,                  (waveshare-screen)
                                  notifyDevice=true)
```

`firstNodeIndex: 2` — mọi bản tin từ `sensor-node` vào chain đều bắt đầu ở **Message Type Switch**, tách làm 3 nhánh song song: lưu timeseries thô, lưu attribute thô, và nhánh xử lý chính (node 3–6).

### 4.2 Danh sách node (tương đương "API" của rule-chain)

| # | Tên node | Loại (ThingsBoard) | Vai trò |
| --- | --- | --- | --- |
| 0 | `Save Timeseries (sensor-node)` | `TbMsgTimeseriesNode` | Lưu lịch sử timeseries thô (`left_front`, `right_front`) cho thiết bị `sensor-node`, không qua xử lý. |
| 1 | `Save Attributes` | `TbMsgAttributesNode` (`CLIENT_SCOPE`) | Lưu bản tin thô làm client attribute của `sensor-node`. |
| 2 | `Message Type Switch` | `TbMsgTypeSwitchNode` | Node vào chain — định tuyến theo loại bản tin (ở đây chỉ dùng nhánh "Post telemetry"). |
| 3 | `Process Ultrasonic & Vehicle Data` | `TbTransformMsgNode` (script `JS`) | **Node xử lý chính** — xem 4.3. |
| 4 | `Change Originator to waveshare-screen` | `TbChangeOriginatorNode` | Đổi chủ thể bản tin từ `sensor-node` sang thiết bị tên `waveshare-screen` (tra theo `entityNamePattern`, không phải theo ID cố định — xem lưu ý 4.4). |
| 5 | `Update Shared Attributes (notifyDevice=true)` | `TbMsgAttributesNode` (`SHARED_SCOPE`) | Đẩy kết quả đã xử lý xuống `waveshare-screen` qua MQTT shared-attributes (`notifyDevice=true` → thiết bị đang subscribe nhận ngay). |
| 6 | `Save Timeseries (waveshare-screen)` | `TbMsgTimeseriesNode` | Lưu lịch sử timeseries cho thiết bị `waveshare-screen`. |

### 4.3 Node xử lý chính — `Process Ultrasonic & Vehicle Data` (JS script)

Input: bản tin telemetry gốc từ `sensor-node`, vd `{"left_front":85.3,"right_front":142.0}` (một trong hai key có thể vắng mặt nếu cảm biến đó mất tín hiệu).

Logic (rút gọn từ `jsScript` trong file JSON):

```js
dist = min(left_front, right_front)              // hoặc giá trị còn lại nếu chỉ 1 key có mặt
vehicle_detected = 0 < dist <= 50                // cm
warning_status = dist < 20  ? "DANGER"
                : vehicle_detected ? "WARNING"
                : "NORMAL"
relay  = vehicle_detected ? "ON" : "OFF"
buzzer = relay                                    // chỉ để hiển thị, còi thật điều khiển cục bộ trên sensor-node
```

Output (`msgType: "POST_ATTRIBUTES_REQUEST"`): `{ vehicle_detected, warning_status, relay, buzzer, source_device: "sensor-node", left_front?, right_front? }`.

> Bảng schema đầy đủ (kiểu, đơn vị, ví dụ) cho payload này và 2 payload MQTT còn lại của hệ thống: [`docs/architecture/DATA_SCHEMA.md` mục 2](architecture/DATA_SCHEMA.md#2-coreiot-rule-chain--waveshare-screen-mqtt-shared-attributes).

**Đổi ngưỡng cảnh báo**: sửa 2 hằng số `50.0` (ngưỡng WARNING/`vehicle_detected`) và `20.0` (ngưỡng DANGER) trực tiếp trong `jsScript` — trên UI CoreIoT: mở node → tab **JavaScript** → sửa số → **Save** → **Save rule chain**. Trên nhánh hiện tại, ngưỡng này **độc lập** với buzzer/UI thật vì rule-chain không nhận dữ liệu — xem [mục 3.4](#34-đổi-ngưỡng-cảnh-báo-warningdanger-buzzer--ui).

**Thêm cảm biến mới** (khi lắp thêm S1/S2/S4/S6): thêm khối `var xxx = typeof msg.xxx !== 'undefined' ? parseFloat(msg.xxx) : null;` cho key mới, đưa vào phép tính `dist = min(...)`, và thêm `if (xxx !== null) { newMsg["xxx"] = xxx; }` ở cuối để `waveshare-screen` nhận được key đó (khớp bảng JSON key ở [`firmware/waveshare-screen/README.md`](../firmware/waveshare-screen/README.md#sensor-layout-truck-no-zone-blind-spot-pattern)).

### 4.4 Lưu ý cấu hình khi deploy trên CoreIoT instance khác

- `Change Originator to waveshare-screen` tra thiết bị đích bằng **tên** (`entityNamePattern: "waveshare-screen"`), không phải theo `additionalInfo.description` (chỉ là ghi chú, chứa Device ID `30287b60-8a67-11f1-84a8-c17e50898235` của instance gốc — **không dùng để định tuyến**, không cần sửa khi đổi tenant). Điều kiện bắt buộc: thiết bị trên CoreIoT phải được đặt tên **chính xác** `waveshare-screen`, nếu không node này sẽ không tìm thấy originator và bản tin bị drop âm thầm.
- Rule chain này **không phải root chain** (`"root": false`) — phải gán làm rule-chain xử lý cho thiết bị `sensor-node` thủ công (**Devices → sensor-node → Manage rule chain**) sau khi import, nếu không bản tin sẽ đi qua Root Rule Chain mặc định thay vì chain này.
- Import file JSON qua **Rule Chains → Import rule chain** sẽ tạo chain mới với `firstRuleNodeId` do CoreIoT tự gán lại — không cần sửa tay trường này trong file.

## 5. Lưu ý bảo mật

`CoreiotConfig.h` và `coreiot_client.h` hiện chứa **Wi-Fi password và MQTT Access Token dạng plaintext, đã commit vào Git** (không phải file `.gitignore` như `config/keys.json`). Đây là chấp nhận được cho quy mô prototype/học thuật hiện tại, nhưng nếu đưa lên môi trường thật hoặc repo public rộng hơn, nên chuyển các giá trị này sang NVS/`config/keys.json` (gitignored) và đọc runtime, thay vì hardcode trong header đã commit.
