# Waveshare Screen Module - ESP32-S3 LVGL Collision-Avoidance Dashboard

Dự án hiển thị đồ họa LVGL dành cho vi điều khiển **ESP32-S3** kết hợp màn hình cảm ứng **Waveshare ESP32-S3 Touch LCD 7 inch** (800x480 RGB). Firmware hiển thị dashboard **cảnh báo va chạm/điểm mù (blind-spot)** cho 6 cảm biến siêu âm JSN-SR04T (FOV 75°) bố trí quanh xe, đồng thời đồng bộ dữ liệu qua **CoreIoT (ThingsBoard) MQTT**.

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

| ID | Vị trí | Offset | JSON key (MQTT) |
| :--- | :--- | :--- | :--- |
| S1 | Front (center) | 0° | `front` |
| S2 | Rear (center) | 180° | `rear` |
| S3 | Left-Front (side, front half) | -90° | `left_front` |
| S4 | Left-Rear (side, rear half) | -90° | `left_rear` |
| S5 | Right-Front (side, front half) | +90° | `right_front` |
| S6 | Right-Rear (side, rear half) | +90° | `right_rear` |

Datasheet cảm biến JSN-SR04T: góc quét (FOV) **75°**, tầm đo hiệu dụng **20cm – 600cm** (giá trị ngoài dải được `sensor_model` tự động clamp).

Ngưỡng cảnh báo: `>100cm` = An toàn (xanh lá), `30-100cm` = Cẩn thận (vàng), `<30cm` = Nguy hiểm (đỏ, nhấp nháy).

---

## 🌐 CoreIoT MQTT Integration

Firmware kết nối MQTT đến server **CoreIoT (ThingsBoard)** qua component `coreiot_client`:

- **MQTT Broker**: `app.coreiot.io` (Port `1883`)
- **Device Access Token**: định nghĩa trong `components/coreiot_client/include/coreiot_client.h` (`COREIOT_MQTT_ACCESS_TOKEN`)
- **Telemetry topic**: `v1/devices/me/telemetry`
- **Định dạng dữ liệu vào**: JSON với 6 khóa `front`, `rear`, `left_front`, `left_rear`, `right_front`, `right_rear` (khoảng cách cm), ví dụ `{"front":120,"rear":180,"left_front":85,"left_rear":150,"right_front":25,"right_rear":200}` — parse trong `main.c:on_mqtt_data()` và đẩy vào `ui_dashboard_update_sensor()`.

---

## 📁 Cấu trúc Mô-đun Dự án (Project Architecture)

```text
waveshare-screen/
├── CMakeLists.txt              # Cấu hình biên dịch dự án root (-Wno-attributes)
├── sdkconfig.defaults          # Cấu hình tối ưu PSRAM, FreeRTOS & LVGL
├── build_and_flash.bat         # Script biên dịch tăng tiến (Incremental Build) & nạp tự động
├── README.md                   # Tài liệu hướng dẫn sử dụng dự án màn hình hiển thị
├── components/                 # Module hóa theo components/ ESP-IDF
│   ├── sensor_model/            # Struct thread-safe (mutex) lưu 6 khoảng cách + góc lắp
│   ├── coreiot_client/          # Wrapper Wi-Fi STA + MQTT CoreIoT (callback-based, tách khỏi UI)
│   └── ui_dashboard/            # Toàn bộ giao diện LVGL Collision Dashboard
└── main/
    ├── CMakeLists.txt          # Đăng ký main + liên kết components
    ├── idf_component.yml       # Quản lý dependency (esp_lvgl_adapter, esp_lcd_touch_gt911, cjson, mqtt)
    ├── main.c                  # app_main(): khởi tạo BSP, LVGL adapter, dashboard, CoreIoT client
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

**`components/coreiot_client/include/coreiot_client.h`**

- `void coreiot_client_set_callbacks(wifi_cb, mqtt_cb, data_cb)` — đăng ký callback trước khi init.
- `void coreiot_client_init(void)` — khởi tạo NVS, Wi-Fi STA, MQTT client.
- `int coreiot_client_publish_telemetry(const char *json_payload)`.
- `bool coreiot_client_has_recent_data(uint32_t max_age_ms)`.

**`components/ui_dashboard/include/ui_dashboard.h`**

- `void ui_dashboard_init(void)` — dựng toàn bộ layout 800x480 (header, tab Collision/System, sidebar trái/phải, canvas xe 2D + 6 arc cảm biến).
- `void ui_dashboard_update_sensor(uint8_t sensor_id, uint16_t dist_cm)` — cập nhật 1 dòng sidebar + màu/nhấp nháy arc + re-evaluate hazard.
- `void ui_dashboard_set_iot_status(bool is_connected, const char *ip)` — cập nhật badge Wi-Fi/MQTT trên header và tab System.
- `void ui_dashboard_set_hazard_warning(bool is_pedestrian_crossing_risk)` — ép cảnh báo "CROSSING TRAFFIC HAZARD" độc lập với logic tự động.

Tất cả hàm `ui_dashboard_*` phải được gọi trong `esp_lv_adapter_lock()/unlock()` vì LVGL không thread-safe.

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

### Log khởi động chuẩn (Golden Boot Log, verified 2026-08-03 on COM9):
```text
I (577) bsp_lcd_port: Install RGB LCD panel driver
I (630) bsp_lcd_port: Initialize RGB LCD panel
I (1032) bsp_lcd_port: Initialize touch controller GT911
I (1038) GT911: TouchPad_ID:0x39,0x31,0x31
I (1046) esp_lvgl:adapter: LVGL adapter initialized successfully
I (1087) collision_dashboard: Initializing Collision-Avoidance Dashboard
I (1175) ui_dashboard: Collision dashboard UI initialized
I (1360) coreiot_client: Wi-Fi STA started, connecting to SSID: ACLAB...
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
