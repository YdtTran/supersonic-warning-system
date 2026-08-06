# Sensor Node Module - ESP32-S3 JSN-SR04T Vehicle Detection (S1 / S3 / S5)

Mô-đun vi điều khiển **ESP32-S3** (board `yolo_uno`, PlatformIO + Arduino framework) đọc 3 cảm biến siêu âm chống nước **JSN-SR04T V3** (Mode 0 mặc định, Trig/Echo qua GPIO — không qua UART), lọc nhiễu bằng thuật toán cụm + EMA, và gửi khoảng cách đã lọc trực tiếp tới `waveshare-screen` qua **ESP-NOW** (đường truyền cục bộ, không qua Wi-Fi AP/MQTT/cloud — xem [`docs/architecture/ESPNOW_NETWORK.md`](../../docs/architecture/ESPNOW_NETWORK.md)).

> Nhánh này **tạm ngắt hẳn** đường CoreIoT/MQTT để tránh xung đột Wi-Fi channel với ESP-NOW. `CoreiotClient.h`/`CoreiotConfig.h` vẫn còn trong cây mã nguồn nhưng **không được include/dùng** — giữ lại để khôi phục sau này nếu cần publish MQTT song song.

Theo sơ đồ bố trí 6 cảm biến của hệ thống (xem [firmware/waveshare-screen/README.md](../waveshare-screen/README.md)), 3 cảm biến trên board này là:

| Index cục bộ | ID hệ thống | Vị trí | Slot ESP-NOW (`SENSOR_ESPNOW_SLOT`) |
| :--- | :--- | :--- | :--- |
| `SENSOR_PINS[0]` | **S1** | Front (chính giữa, trước) | `ESPNOW_SLOT_FRONT` (0) |
| `SENSOR_PINS[1]` | **S3** | Left-Front (hông trái, nửa trước) | `ESPNOW_SLOT_LEFT_FRONT` (2) |
| `SENSOR_PINS[2]` | **S5** | Right-Front (hông phải, nửa trước) | `ESPNOW_SLOT_RIGHT_FRONT` (4) |

---

## Cấu trúc Mô-đun

```text
firmware/sensor-node/
├── platformio.ini              # Board yolo_uno (ESP32-S3), lib_deps (PubSubClient, ...)
├── include/
│   ├── Config.h                 # SENSOR_PINS, tham số đo + bộ lọc cụm/EMA
│   ├── EspNowConfig.h           # Channel/MAC đích/struct espnow_sensor_msg_t/ánh xạ SENSOR_ESPNOW_SLOT
│   ├── CoreiotConfig.h          # (không dùng trên nhánh này) WiFi SSID/Pass, broker/token/topic CoreIoT
│   ├── UltrasonicSensor.h       # Đọc 1 cảm biến JSN-SR04T (ngắt Echo, non-blocking)
│   ├── DistanceFilter.h         # Bộ lọc cụm + EMA cho từng cảm biến
│   ├── SharedState.h            # Trạng thái khoảng cách đã lọc, chia sẻ giữa các task
│   ├── EspNowClient.h           # Wrapper esp_now gửi trực tiếp tới waveshare-screen
│   └── CoreiotClient.h          # (không dùng trên nhánh này) Wrapper WiFi + MQTT tới CoreIoT
└── src/
    ├── main.cpp                 # sensorTask (đo/lọc), appTask (ví dụ), networkTask (gửi ESP-NOW)
    ├── UltrasonicSensor.cpp
    ├── DistanceFilter.cpp
    ├── SharedState.cpp
    ├── EspNowClient.cpp
    └── CoreiotClient.cpp        # (không dùng trên nhánh này)
```

---

## Kết nối ESP-NOW (waveshare-screen)

- **Không kết nối Wi-Fi AP/MQTT** — `WiFi.mode(WIFI_STA)` chỉ để lấy cơ chế Wi-Fi radio cho ESP-NOW, không có `WiFi.begin()`/broker nào cả trên nhánh này.
- **Channel**: cố định `ESPNOW_CHANNEL = 1` trong `include/EspNowConfig.h` — phải khớp với `waveshare-screen`.
- **MAC đích**: `ESPNOW_PEER_MAC` trong `include/EspNowConfig.h` — MAC của board `waveshare-screen` (receiver). Cập nhật lại nếu board đó bị reflash/đổi. Nguồn thông tin dùng chung đầy đủ (bảng MAC 2 board, cách đồng bộ lại): [`docs/architecture/ESPNOW_NETWORK.md`](../../docs/architecture/ESPNOW_NETWORK.md).
- **Định dạng dữ liệu**: struct nhị phân packed `espnow_sensor_msg_t` (6 slot `distance_cm`/`valid`, không phải JSON) — chỉ 3 slot có phần cứng thật (`front`, `left_front`, `right_front`, ánh xạ qua `SENSOR_ESPNOW_SLOT[]`) được set `valid=1`; slot nào cảm biến mất tín hiệu hoặc chưa lắp phần cứng giữ `valid=0` thay vì gửi `0` giả.
- **Tần suất gửi**: mỗi `ESPNOW_SEND_INTERVAL_MS` = **500ms** (2Hz) — tách biệt với tốc độ đo/lọc cục bộ (`MEASURE_INTERVAL_MS` trong `Config.h` = 100ms) để bộ lọc cụm/EMA vẫn phản ứng nhanh cho cảnh báo va chạm, chỉ giảm tần suất gửi lên mạng.
- `waveshare-screen` nhận và tự đánh giá hazard cục bộ (không còn Rule-Chain CoreIoT trên nhánh này) — xem `firmware/waveshare-screen/src/main.c` và `ui_dashboard.c`.

## Kiến trúc Task (FreeRTOS trên Arduino core)

| Task | Core | Priority | Nhiệm vụ |
| :--- | :--- | :--- | :--- |
| `sensorTask` | 1 | 2 | Đo + lọc từng cảm biến mỗi `MEASURE_INTERVAL_MS`, ghi vào `SharedState` |
| `appTask` | 1 | 1 | Ví dụ dùng `SharedState` (bật LED khi vật ở gần) |
| `networkTask` | 0 | 1 | Đóng gói `SharedState` thành `espnow_sensor_msg_t`, gửi tới `waveshare-screen` mỗi `ESPNOW_SEND_INTERVAL_MS` |
| `buzzerTask` | — | — | Đọc khoảng cách gần nhất từ `SharedState`, điều khiển buzzer (GPIO `BUZZER_PIN`) cục bộ |

`networkTask` chạy trên core 0 (tách khỏi core 1) để việc gửi ESP-NOW không ảnh hưởng timing đo cảm biến (Echo timeout tính bằng micro-giây).

## Buzzer cảnh báo (cục bộ, không qua mạng)

`buzzerTask` (`src/main.cpp`) đọc khoảng cách gần nhất trong `SharedState` và điều khiển trực tiếp buzzer vật lý gắn ở `BUZZER_PIN` (GPIO 48, xem `include/Config.h`) — **không** round-trip qua mạng/cloud, để giữ độ trễ phản hồi thấp nhất cho cảnh báo va chạm:

| Ngưỡng | Khoảng cách | Nhịp beep |
| :--- | :--- | :--- |
| WARNING | `<= BUZZER_WARNING_DISTANCE_CM` (50cm) | mỗi 3000ms (`BUZZER_WARNING_PERIOD_MS`) |
| DANGER | `< BUZZER_DANGER_DISTANCE_CM` (20cm) | mỗi 1000ms (`BUZZER_DANGER_PERIOD_MS`) |

Mỗi lần beep kéo dài `BUZZER_BEEP_ON_MS` = 120ms. Trên nhánh này, đây là ngưỡng buzzer **duy nhất** — không còn field `buzzer` mirror từ Rule-Chain CoreIoT vì `waveshare-screen` không còn nhận dữ liệu qua MQTT.

## Build & Flash (PlatformIO)

```bash
cd firmware/sensor-node
pio run -e yolo_uno          # build
pio run -e yolo_uno -t upload  # flash
pio device monitor -b 115200   # xem log Serial
```
