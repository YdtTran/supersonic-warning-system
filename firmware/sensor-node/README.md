# Sensor Node Module - ESP32-S3 JSN-SR04T Vehicle Detection (S3 / S5)

Mô-đun vi điều khiển **ESP32-S3** (board `yolo_uno`, PlatformIO + Arduino framework) đọc 2 cảm biến siêu âm chống nước **JSN-SR04T** (Trig/Echo), lọc nhiễu bằng thuật toán cụm + EMA, và đẩy khoảng cách đã lọc lên **CoreIoT (ThingsBoard)** qua MQTT.

Theo sơ đồ bố trí 6 cảm biến của hệ thống (xem [firmware/waveshare-screen/README.md](../waveshare-screen/README.md)), 2 cảm biến trên board này là:

| Index cục bộ | ID hệ thống | Vị trí | Key MQTT |
| :--- | :--- | :--- | :--- |
| `SENSOR_PINS[0]` | **S3** | Left-Front (hông trái, nửa trước) | `left_front` |
| `SENSOR_PINS[1]` | **S5** | Right-Front (hông phải, nửa trước) | `right_front` |

---

## Cấu trúc Mô-đun

```text
firmware/sensor-node/
├── platformio.ini              # Board yolo_uno (ESP32-S3), lib_deps (PubSubClient, ...)
├── include/
│   ├── Config.h                 # SENSOR_PINS, tham số đo + bộ lọc cụm/EMA
│   ├── CoreiotConfig.h          # WiFi SSID/Pass, broker/token/topic CoreIoT, publish rate
│   ├── UltrasonicSensor.h       # Đọc 1 cảm biến JSN-SR04T (ngắt Echo, non-blocking)
│   ├── DistanceFilter.h         # Bộ lọc cụm + EMA cho từng cảm biến
│   ├── SharedState.h            # Trạng thái khoảng cách đã lọc, chia sẻ giữa các task
│   └── CoreiotClient.h          # Wrapper WiFi + MQTT (PubSubClient) tới CoreIoT
└── src/
    ├── main.cpp                 # sensorTask (đo/lọc), appTask (ví dụ), networkTask (publish MQTT)
    ├── UltrasonicSensor.cpp
    ├── DistanceFilter.cpp
    ├── SharedState.cpp
    └── CoreiotClient.cpp
```

---

## Kết nối CoreIoT

- **MQTT Broker**: `app.coreiot.io:1883` (định nghĩa trong `include/CoreiotConfig.h`)
- **Device Access Token**: `COREIOT_MQTT_TOKEN` trong `include/CoreiotConfig.h` — phải trùng với `SENSOR_NODE_DEVICE_TOKEN` trong `config/keys.json` (gitignored, dùng bởi `tools/test_mqtt_coreiot.py`). Nếu cấp lại token trên CoreIoT, cập nhật cả 2 nơi.
- **Telemetry topic**: `v1/devices/me/telemetry`
- **Định dạng dữ liệu**: JSON với các key `left_front`, `right_front` (khoảng cách cm), ví dụ `{"left_front":85.3,"right_front":142.0}`. Key nào mất tín hiệu (cảm biến reject/timeout) sẽ bị bỏ qua thay vì gửi giá trị `0` giả.
- **Tần suất publish**: mỗi `COREIOT_PUBLISH_INTERVAL_MS` = **500ms** (2Hz) — tách biệt với tốc độ đo/lọc cục bộ (`MEASURE_INTERVAL_MS` trong `Config.h` = 100ms) để bộ lọc cụm/EMA vẫn phản ứng nhanh cho cảnh báo va chạm, chỉ giảm tần suất gửi lên mạng.
- Rule chain phía CoreIoT xử lý payload này: `cloud/coreiot/rule_chain/supersonic_rule_chain.json`.

## Kiến trúc Task (FreeRTOS trên Arduino core)

| Task | Core | Priority | Nhiệm vụ |
| :--- | :--- | :--- | :--- |
| `sensorTask` | 1 | 2 | Đo + lọc từng cảm biến mỗi `MEASURE_INTERVAL_MS`, ghi vào `SharedState` |
| `appTask` | 1 | 1 | Ví dụ dùng `SharedState` (bật LED khi vật ở gần) |
| `networkTask` | 0 | 1 | Kết nối WiFi/MQTT, publish `SharedState` lên CoreIoT mỗi `COREIOT_PUBLISH_INTERVAL_MS` |

`networkTask` chạy trên core 0 (tách khỏi core 1) để việc kết nối lại WiFi/MQTT hoặc publish không ảnh hưởng timing đo cảm biến (Echo timeout tính bằng micro-giây).

## Build & Flash (PlatformIO)

```bash
cd firmware/sensor-node
pio run -e yolo_uno          # build
pio run -e yolo_uno -t upload  # flash
pio device monitor -b 115200   # xem log Serial
```
