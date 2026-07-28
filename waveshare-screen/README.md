# Waveshare Screen Module - ESP32-S3 LVGL Graphics Application

Dự án hiển thị đồ họa LVGL dành cho vi điều khiển **ESP32-S3** kết hợp màn hình cảm ứng **Waveshare ESP32-S3 Touch LCD 7** (800x480 RGB), chạy trên nền tảng **ESP-IDF v6.0.2**.

---

## 🛠 Thống kê Phần cứng (Hardware Specs)

| Thành phần | Thông số kĩ thuật |
| :--- | :--- |
| **Microcontroller** | ESP32-S3 (Dual-Core 240MHz, 8MB Octal PSRAM, 8MB Flash) |
| **Màn hình** | 7.0" RGB LCD, Độ phân giải **800 x 480**, 16-bit color (RGB565) |
| **Đèn nền (Backlight)** | Điều khiển qua IC mở rộng IO CH422G (I2C Address `0x38` / `0x24`) |
| **Cảm ứng (Touch)** | GT911 Capacitive Touch Controller (I2C Fast-mode `400kHz`) |
| **Cổng Nạp Mặc định** | **`COM9`** |

---

## 📁 Cấu trúc Mô-đun Dự án (Project Architecture)

Dự án được phân chia mô-đun hóa sạch sẽ, tách biệt phần cứng (BSP) và giao diện (UI):

```text
waveshare-screen/
├── CMakeLists.txt              # Cấu hình biên dịch dự án root (-Wno-attributes)
├── sdkconfig.defaults          # Cấu hình tối ưu PSRAM, FreeRTOS & LVGL
├── build_and_flash.bat         # Script biên dịch tăng tiến (Incremental Build) & nạp tự động
├── README.md                   # Tài liệu hướng dẫn sử dụng dự án
├── version_logs/               # Thư mục lưu vết các phiên bản & lịch sử sửa lỗi
│   └── VERSION_LOG.md          # Chi tiết các phiên bản & giải pháp vấn đề kĩ thuật
└── main/
    ├── CMakeLists.txt          # Đăng ký mô-đun BSP và UI vào hệ thống build ESP-IDF
    ├── idf_component.yml       # Quản lý dependency (esp_lvgl_adapter, esp_lcd_touch_gt911)
    ├── main.c                  # Luồng chính kết nối BSP và UI
    ├── bsp/                    # Board Support Package (Màn hình & Cảm ứng)
    │   ├── waveshare_rgb_lcd_port.h # Khai báo chân GPIO, timing 16MHz & hàm driver
    │   └── waveshare_rgb_lcd_port.c # Driver RGB panel, I2C master bus & CH422G
    └── ui/                     # User Interface & Animation Engine
        ├── ui_app.h / ui_app.c      # Khởi tạo canvas, style nền slate & nhãn "ACLAB 2023"
        └── ui_anim.h / ui_anim.c    # Bộ công cụ animation di chuyển mượt từ trên xuống
```

---

## 🚀 Hướng dẫn Biên dịch & Nạp Firmware (Build & Flash)

Dự án hỗ trợ script `build_and_flash.bat` tự động nhận diện môi trường ESP-IDF v6.0.2 và **biên dịch tăng tiến (Incremental Build)** chỉ mất 2-3 giây.

### 1. Biên dịch và Nạp tự động (Cổng COM9)
```cmd
cd waveshare-screen
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

## 🌐 CoreIoT MQTT Integration (Server Data Fetch)
Mô-đun Waveshare Screen sử dụng kết nối MQTT đến server **CoreIoT (ThingsBoard)** để nhận dữ liệu cảm biến và hiển thị trên màn hình:

- **MQTT Broker**: `app.coreiot.io` (Port `1883`)
- **Device Access Token (Key)**: `<COREIOT_DEVICE_TOKEN>` *(Cấu hình tại file `AGENTS.md` local, không commit lên git)*
- **Chức năng**: Kết nối tới CoreIoT server, nhận dữ liệu (Telemetry/Attributes) từ server và hiển thị các chỉ số khoảng cách (cm), mức cảnh báo trên giao diện đồ họa LVGL.


---

## 🔍 Chẩn đoán Lỗi & Trích xuất Log (Serial Debugging)

Khi màn hình bị tối đen hoặc vi điều khiển gặp sự cố, sử dụng công cụ bắt log Python (thay thế cho `idf_monitor.py` trong môi trường non-TTY):

```cmd
C:\Espressif\tools\python\v6.0.2\venv\Scripts\python.exe ..\.agents\skills\esp32_screen_debug\scripts\read_serial.py COM9 5
```

### Log khởi động chuẩn (Golden Boot Log):
```text
I (426) bsp_lcd_port: Install RGB LCD panel driver
I (479) bsp_lcd_port: Initialize RGB LCD panel
I (887) GT911: TouchPad_ID: 0x39, 0x31, 0x31
I (948) main: Initializing ACLAB 2023 UI Application
I (949) ui_app: ACLAB 2023 UI initialized successfully
```

---

## 📋 Lịch sử Các Lỗi Đã Xử Lý (Troubleshooting Reference)

Chi tiết đầy đủ xem tại [version_logs/VERSION_LOG.md](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/version_logs/VERSION_LOG.md):
- **`ISSUE-01`**: Thêm `-Wno-attributes` xử lý xung đột GCC 15 macro.
- **`ISSUE-02`**: Cập nhật struct `esp_lcd_rgb_panel_config_t` chuẩn ESP-IDF v6.0.2.
- **`ISSUE-03`**: Khắc phục lỗi `0/1961` rebuild lại từ đầu bằng cách bỏ lệnh `set-target` lặp lại.
- **`ISSUE-04`**: Chuyển toàn bộ I2C sang `driver/i2c_master.h` triệt tiêu lỗi kernel panic `CONFLICT! driver_ng`.
- **`ISSUE-05`**: Đặt tần số SCL I2C cố định `400kHz` tránh lỗi `ESP_ERR_INVALID_ARG`.
