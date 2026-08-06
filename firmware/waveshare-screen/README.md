# Waveshare Screen Module - ESP32-S3 LVGL Collision-Avoidance Dashboard

Dự án hiển thị đồ họa LVGL dành cho vi điều khiển **ESP32-S3** kết hợp màn hình cảm ứng **Waveshare ESP32-S3 Touch LCD 7 inch** (800x480 RGB). Firmware hiển thị dashboard **cảnh báo va chạm/điểm mù (blind-spot)** cho 6 cảm biến siêu âm JSN-SR04T (FOV 75°) bố trí quanh xe, nhận dữ liệu trực tiếp từ `firmware/sensor-node` qua **ESP-NOW** (không qua Wi-Fi AP/cloud — xem [`docs/architecture/ESPNOW_NETWORK.md`](../../docs/architecture/ESPNOW_NETWORK.md)) và tự đánh giá hazard cục bộ. Component `coreiot_client` (CoreIoT/MQTT) vẫn còn trong cây mã nguồn nhưng **không được gọi trên nhánh hiện tại**.

---

## 🛠 Thống kê Phần cứng (Hardware Specs)

| Thành phần | Thông số kĩ thuật |
| :--- | :--- |
| **Microcontroller** | ESP32-S3 (Dual-Core 240MHz, 8MB Octal PSRAM, 8MB Flash) |
| **Màn hình** | 7.0" RGB LCD, Độ phân giải **800 x 480**, 16-bit color (RGB565) |
| **Đèn nền (Backlight)** | Điều khiển qua IC mở rộng IO CH422G (I2C Address `0x38` / `0x24`) |
| **Cảm ứng (Touch)** | GT911 Capacitive Touch Controller (I2C Fast-mode `400kHz`) |
| **Cảm biến khoảng cách** | 6 x JSN-SR04T (FOV 75°, tầm đo 20cm–6m), bố trí kiểu "No-Zone" xe tải: 1 trước, 1 sau, 2 mỗi bên (xem bảng bên dưới) |
| **Cổng Nạp Mặc định** | **`COM9`** |

### RGB LCD & Touch Pinout (ESP32-S3, xem `main/bsp/waveshare_rgb_lcd_port.h`)

| Tín hiệu | GPIO | Tín hiệu | GPIO |
| :--- | :--- | :--- | :--- |
| VSYNC | 3 | DATA8 | 48 |
| HSYNC | 46 | DATA9 | 47 |
| DE | 5 | DATA10 | 21 |
| PCLK | 7 | DATA11 | 1 |
| DATA0 | 14 | DATA12 | 2 |
| DATA1 | 38 | DATA13 | 42 |
| DATA2 | 18 | DATA14 | 41 |
| DATA3 | 17 | DATA15 | 40 |
| DATA4 | 10 | I2C SCL (CH422G/GT911) | 9 |
| DATA5 | 39 | I2C SDA (CH422G/GT911) | 8 |
| DATA6 | 0 | Touch Reset | 4 |
| DATA7 | 45 | | |

### Sensor Layout (truck "No-Zone" blind-spot pattern)

Bố trí theo sơ đồ điểm mù xe tải chuẩn: 1 cảm biến giữa đầu xe (Zone 1 - Front), 1 cảm biến giữa đuôi xe (Zone 2 - Rear), và mỗi bên hông 2 cảm biến phủ nửa trước + nửa sau (Zone 3 - Left, Zone 4 - Right).

| ID | Vị trí | Offset | Slot ESP-NOW |
| :--- | :--- | :--- | :--- |
| S1 | Front (center) | 0° | `front` (0) |
| S2 | Rear (center) | 180° | `rear` (1) |
| S3 | Left-Front (side, front half) | -90° | `left_front` (2) |
| S4 | Left-Rear (side, rear half) | -90° | `left_rear` (3) |
| S5 | Right-Front (side, front half) | +90° | `right_front` (4) |
| S6 | Right-Rear (side, rear half) | +90° | `right_rear` (5) |

Datasheet cảm biến JSN-SR04T: góc quét (FOV) **75°**, tầm đo hiệu dụng **20cm – 600cm** (giá trị ngoài dải được `sensor_model` tự động clamp).

Ngưỡng cảnh báo: `>100cm` = An toàn (xanh lá), `30-100cm` = Cẩn thận (vàng), `<30cm` = Nguy hiểm (đỏ, nhấp nháy).

> **Trạng thái hiện tại**: `firmware/sensor-node` có phần cứng ở S1 (`front`), S3 (`left_front`), S5 (`right_front`) và gửi dữ liệu thật qua ESP-NOW. UI dashboard vẫn dựng đủ layout cho 6 cảm biến/4 zone để sẵn sàng mở rộng, nhưng 3 zone còn lại (rear/left_rear/right_rear) hiện hiển thị "no data" (arc xám, "-- cm") cho tới khi có board tương ứng — xem [`docs/architecture/DATA_SCHEMA.md` mục 4](../../docs/architecture/DATA_SCHEMA.md#4-khoảng-trống-đã-biết-chỉ-36-slot-sensor_model-có-dữ-liệu-sống).

---

## 🔊 Buzzer (độc lập trên `sensor-node`)

Buzzer **vật lý** được điều khiển hoàn toàn cục bộ trên `sensor-node` (GPIO48, không qua ESP-NOW hay bất kỳ round-trip mạng nào, để giữ độ trễ thấp) — xem [`firmware/sensor-node/README.md`](../sensor-node/README.md#buzzer-cảnh-báo-cục-bộ-không-qua-mạng). `waveshare-screen` **không nhận được trạng thái buzzer thật** trên nhánh hiện tại (đường MQTT `relay`/`buzzer` mirror cũ từ Rule-Chain CoreIoT không còn hoạt động); `ui_dashboard_set_buzzer_state()` vẫn tồn tại trong API nhưng hiện không có gì gọi nó.

---

## 🌐 ESP-NOW Integration (đang dùng)

Firmware nhận dữ liệu trực tiếp từ `sensor-node` qua **ESP-NOW**, không kết nối Wi-Fi AP/MQTT nào. Logic nhận nằm thẳng trong `src/main.c` (không phải 1 component riêng như `coreiot_client` cũ):

- **Channel**: cố định (`ESPNOW_CHANNEL`, khớp `sensor-node`) — set qua `esp_wifi_set_channel()`, không có `esp_wifi_connect()`.
- **Cơ chế**: `esp_now_register_recv_cb(on_data_recv)` — chỉ lắng nghe, không cần `esp_now_add_peer()` vì không gửi ngược lại `sensor-node`.
- **Định dạng dữ liệu vào**: struct nhị phân packed `espnow_sensor_msg_t` (6 slot `distance_cm`/`valid`, không phải JSON) — `on_data_recv()` gọi `ui_dashboard_update_sensor()` cho slot `valid=1`, `ui_dashboard_clear_sensor()` cho slot `valid=0`.
- **Watchdog liên kết**: `esp_timer` 1s kiểm tra thời điểm nhận gần nhất, gọi `ui_dashboard_set_espnow_status(false)` (badge "ESP-NOW: NO LINK") nếu quá 1.5s không nhận được message nào.
- Schema đầy đủ + cách đồng bộ MAC khi đổi board: [`docs/architecture/ESPNOW_NETWORK.md`](../../docs/architecture/ESPNOW_NETWORK.md).

### CoreIoT MQTT Integration (không dùng trên nhánh hiện tại)

Component `coreiot_client` (`components/coreiot_client/`) vẫn còn trong cây mã nguồn — wrapper Wi-Fi STA + MQTT tới CoreIoT (`app.coreiot.io:1883`), parse JSON key `left_front`/`right_front`/`relay`/`buzzer`. Không được gọi từ `src/main.c` trên nhánh này; giữ lại để khôi phục sau nếu cần đường cloud song song với ESP-NOW.

---

## 📁 Cấu trúc Mô-đun Dự án (Project Architecture)

PlatformIO-hoá (`platformio.ini`, `board = yolo_uno`) nhưng dùng `framework = espidf` **thuần** (không Arduino) — xem lý do và giới hạn kỹ thuật tại [docs/logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md](../../docs/logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md). PlatformIO's ESP-IDF builder luôn đòi hỏi thư mục gốc `src/` (không phải `main/` kiểu ESP-IDF chuẩn), nên thư mục `main/` cũ đã được đổi tên thành `src/`, nội dung giữ nguyên.

```text
waveshare-screen/
├── platformio.ini              # env:yolo_uno, framework = espidf, board_build.partitions = partitions.csv
├── boards/yolo_uno.json        # Board definition (copy từ firmware/sensor-node)
├── CMakeLists.txt              # Cấu hình biên dịch dự án root (-Wno-attributes)
├── sdkconfig.defaults          # Cấu hình tối ưu PSRAM, FreeRTOS & LVGL
├── partitions.csv              # Bảng phân vùng flash
├── build_and_flash.bat         # Script gọi pio run/pio device monitor (build/flash/monitor/all/clean)
├── README.md                   # Tài liệu hướng dẫn sử dụng dự án màn hình hiển thị
├── components/                 # Module hóa theo components/ ESP-IDF
│   ├── sensor_model/            # Struct thread-safe (mutex) lưu 6 khoảng cách + góc lắp
│   ├── coreiot_client/          # (không dùng trên nhánh hiện tại) Wrapper Wi-Fi STA + MQTT CoreIoT
│   └── ui_dashboard/            # Toàn bộ giao diện LVGL Collision Dashboard (tab COLLISION + SYSTEM)
└── src/                         # PlatformIO PROJECT_SRC_DIR (trước đây là main/, đổi tên do PlatformIO yêu cầu)
    ├── CMakeLists.txt          # Đăng ký main + liên kết components
    ├── idf_component.yml       # Quản lý dependency (esp_lvgl_adapter, esp_lcd_touch_gt911, cjson, mqtt)
    ├── main.c                  # app_main(): khởi tạo BSP, LVGL adapter, dashboard;
    │                           # networkTask riêng (core 0, FreeRTOS): esp_now_register_recv_cb() nhận ESP-NOW
    └── bsp/                    # Board Support Package (Màn hình & Cảm ứng)
        ├── waveshare_rgb_lcd_port.h # Khai báo chân GPIO, timing 16MHz & hàm driver
        └── waveshare_rgb_lcd_port.c # Driver RGB panel, I2C master bus & CH422G
```

### API Reference

**`components/sensor_model/include/sensor_model.h`**
- `void sensor_model_init(void)` — khởi tạo mutex + giá trị mặc định.
- `void sensor_model_set_distance(sensor_id_t id, uint16_t distance_cm)` — cập nhật 1 cảm biến (thread-safe).
- `sensor_reading_t sensor_model_get(sensor_id_t id)` / `sensor_model_get_all(...)` — đọc snapshot (thread-safe).
- `sensor_zone_t sensor_model_classify(uint16_t distance_cm)` — phân loại SAFE/CAUTION/DANGER.

**`components/coreiot_client/include/coreiot_client.h`** *(không dùng trên nhánh hiện tại)*

- `void coreiot_client_set_callbacks(wifi_cb, mqtt_cb, data_cb)` — đăng ký callback trước khi init.
- `void coreiot_client_init(void)` — khởi tạo NVS, Wi-Fi STA, MQTT client.
- `int coreiot_client_publish_telemetry(const char *json_payload)`.
- `bool coreiot_client_has_recent_data(uint32_t max_age_ms)`.

**`components/ui_dashboard/include/ui_dashboard.h`**

- `void ui_dashboard_init(void)` — dựng toàn bộ layout 800x480 (header, tab COLLISION/SYSTEM, sidebar trái/phải, canvas xe 2D + 6 arc cảm biến).
- `void ui_dashboard_update_sensor(uint8_t sensor_id, uint16_t dist_cm)` — cập nhật 1 dòng sidebar + màu/nhấp nháy arc + re-evaluate hazard.
- `void ui_dashboard_clear_sensor(uint8_t sensor_id)` — đánh dấu "no data" cho 1 cảm biến (ESP-NOW báo `valid=0`): sidebar về "-- cm", arc màu xám trung tính, re-evaluate hazard.
- `void ui_dashboard_set_iot_status(bool is_connected, const char *ip)` — *(không dùng trên nhánh hiện tại)* badge Wi-Fi/MQTT — đường CoreIoT.
- `void ui_dashboard_set_espnow_status(bool linked)` — cập nhật badge header "ESP-NOW: LINKED"/"ESP-NOW: NO LINK".
- `void ui_dashboard_set_hazard_warning(bool is_pedestrian_crossing_risk)` — ép cảnh báo "CROSSING TRAFFIC HAZARD" độc lập với logic tự động.
- `void ui_dashboard_set_buzzer_state(bool buzzer_on)` — cập nhật dòng `BUZZER: ON/OFF` ở sidebar; hiện không có gì gọi hàm này trên nhánh này (xem mục Buzzer ở trên).

Tab **SYSTEM** hiển thị đầy đủ: device ID, firmware version, ESP-IDF version (`esp_get_idf_version()`), build timestamp, flash size (`esp_flash_get_size`), free/min-free heap, và uptime — tự refresh định kỳ. Các trường CoreIoT broker/access token trong tab này phản ánh cấu hình `coreiot_client` cũ, không phải kết nối đang hoạt động.

Tất cả hàm `ui_dashboard_*` phải được gọi trong `esp_lv_adapter_lock()/unlock()` vì LVGL không thread-safe. Callback nhận ESP-NOW (`on_data_recv`, chạy trên task/context của `esp_now`, khác task LVGL) dùng `esp_lv_adapter_lock(100)` (timeout 100ms) thay vì `esp_lv_adapter_lock(-1)` (chờ vô hạn) — tránh deadlock toàn hệ thống nếu task LVGL bị kẹt.

---

## 🚀 Hướng dẫn Biên dịch & Nạp Firmware (Build & Flash)

Dự án hỗ trợ script `build_and_flash.bat` tự động nhận diện môi trường ESP-IDF v6.0.2 và **biên dịch tăng tiến (Incremental Build)** chỉ mất 2-3 giây.

### 1. Biên dịch và Nạp tự động (Cổng COM9)
```cmd
cd firmware/waveshare-screen
build_and_flash.bat
```

### 2. Biên dịch, Nạp và Mở Serial Monitor
```cmd
build_and_flash.bat all COM9
```

### 3. Các thao tác lẻ khác
```cmd
build_and_flash.bat flash COM9     :: Chỉ nạp binary xuống thiết bị
build_and_flash.bat monitor COM9   :: Xem log UART ở 115200 baud
build_and_flash.bat clean          :: Xóa sạch thư mục build/
```

---

## 🔍 Chẩn đoán Lỗi & Trích xuất Log (Serial Debugging)

Khi màn hình bị tối đen hoặc vi điều khiển gặp sự cố, sử dụng công cụ bắt log Python (thay thế cho `idf_monitor.py` trong môi trường non-TTY):

```cmd
C:\Espressif\tools\python\v6.0.2\venv\Scripts\python.exe .agents\skills\esp32_screen_debug\scripts\read_serial.py COM9 5
```

### Log khởi động chuẩn (Golden Boot Log, verified 2026-08-03 on COM9)

> Log này được ghi **trước khi chuyển sang ESP-NOW** — dòng cuối `coreiot_client: Wi-Fi STA started...` không còn xuất hiện trên nhánh hiện tại, thay bằng log init ESP-NOW trong `main.c`.

```text
I (577) bsp_lcd_port: Install RGB LCD panel driver
I (630) bsp_lcd_port: Initialize RGB LCD panel
I (1032) bsp_lcd_port: Initialize touch controller GT911
I (1038) GT911: TouchPad_ID:0x39,0x31,0x31
I (1046) esp_lvgl:adapter: LVGL adapter initialized successfully
I (1087) collision_dashboard: Initializing Collision-Avoidance Dashboard
I (1175) ui_dashboard: Collision dashboard UI initialized
```
Cảnh báo `alloc partial draw buffer ... failed` / `tear mode 4 setup failed, falling back to allocated buffers` là non-fatal (adapter tự động fallback sang buffer cấp phát thường).

---

## 📋 Lịch sử Các Lỗi Đã Xử Lý (Troubleshooting Reference)

Chi tiết đầy đủ xem tại [docs/logs/WAVESHARE_SCREEN_VERSION_LOG.md](file:///e:/supersonic-sensor-ACLAB/docs/logs/WAVESHARE_SCREEN_VERSION_LOG.md):
- **`ISSUE-01`**: Thêm `-Wno-attributes` xử lý xung đột GCC 15 macro.
- **`ISSUE-02`**: Cập nhật struct `esp_lcd_rgb_panel_config_t` chuẩn ESP-IDF v6.0.2.
- **`ISSUE-03`**: Khắc phục lỗi `0/1961` rebuild lại từ đầu bằng cách bỏ lệnh `set-target` lặp lại.
- **`ISSUE-04`**: Chuyển toàn bộ I2C sang `driver/i2c_master.h` triệt tiêu lỗi kernel panic `CONFLICT! driver_ng`.
- **`ISSUE-05`**: Đặt tần số SCL I2C cố định `400kHz` tránh lỗi `ESP_ERR_INVALID_ARG`.
- **`ISSUE-06`**: Xây lại `waveshare-screen/main` sau khi tái cấu trúc repo (`firmware/waveshare-screen`) làm build directory trỏ sai path — chạy `idf.py fullclean` / xóa `build/` rồi build lại.
