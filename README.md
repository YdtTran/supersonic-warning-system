# Vehicle Detection & Warning System using JSN-SR04T Supersonic Sensor & CoreIoT (ACLAB)

Hệ thống nhúng vi điều khiển **ESP32-S3** phát hiện xe thông qua **cảm biến siêu âm chống nước JSN-SR04T**. Dữ liệu khoảng cách và trạng thái phát hiện xe được gửi lên đám mây **CoreIoT (ThingsBoard)** qua Wi-Fi. Thông qua **Rule-Chain** xử lý sự kiện trên CoreIoT, thông tin phát hiện xe và mức cảnh báo được định tuyến tự động đến mô-đun màn hình cảm ứng **Waveshare ESP32-S3 Touch LCD 7 inch** (`waveshare-screen`) để hiển thị mượt mà trên giao diện đồ họa LVGL.

---

## 🔄 Luồng Dữ liệu & Kiến trúc Hệ thống (Data Flow & Architecture)

```text
 ┌────────────────────────┐
 │   Cảm biến JSN-SR04T   │ (Cảm biến siêu âm chống nước đo khoảng cách xe)
 └───────────┬────────────┘
             │ (Echo / Trig GPIO)
             ▼
 ┌────────────────────────┐
 │  ESP32-S3 Sensor Node  │ (firmware/sensor-node: Xử lý khoảng cách, phát hiện xe)
 └───────────┬────────────┘
             │ (Wi-Fi MQTT Telemetry: v1/devices/me/telemetry)
             ▼
 ┌────────────────────────┐
 │  CoreIoT Cloud Server  │ (app.coreiot.io: Xử lý ngưỡng qua Rule-Chain)
 └───────────┬────────────┘
             │ (MQTT Subscribe / Rule-Chain Forward)
             ▼
 ┌────────────────────────┐
 │ Waveshare Screen Node  │ (firmware/waveshare-screen: Hiển thị cảnh báo trực quan trên LCD 7")
 └────────────────────────┘
```

---

## 📐 Kiến trúc Dự án (Repository Architecture)

```text
supersonic-sensor-ACLAB/
├── AGENTS.template.md                 # Template cấu hình môi trường & quy tắc cho AI Agent (Cross-Device)
├── AGENTS.md                          # Cấu hình môi trường local & cổng COM (Ignored by Git)
├── config/                            # Thư mục quản lý Secret Keys & Cấu hình
│   ├── keys.template.json             # Template mẫu khai báo key cho máy mới (Tracked)
│   └── keys.json                      # Cấu hình chứa Device Access Token local (Ignored)
├── .gitignore                         # Cấu hình bỏ qua build artifacts, secret keys & file cá nhân
├── .agents/                           # Thư mục chứa Custom Skills cho AI Agents
│   └── skills/
│       ├── cross_device_reconfig/     # Skill tự động quét & cấu hình project trên máy mới
│       ├── esp32_screen_debug/        # Skill chẩn đoán lỗi hiển thị, I2C driver & trích xuất log
│       └── lvgl_v9_warning_display/   # Skill tra cứu kiến trúc LVGL v9 / pipeline UI cảnh báo
├── tools/                             # Công cụ kiểm thử MQTT & đọc/vẽ dữ liệu UART
│   ├── test_mqtt_coreiot.py           # Script mô phỏng phát dữ liệu xe & kiểm tra kết nối CoreIoT
│   └── plot_ultrasonic_distance.py    # Vẽ đồ thị real-time giá trị khoảng cách đọc qua UART
├── firmware/                          # Firmware chính thức (production, PlatformIO cho cả 2 board)
│   ├── sensor-node/                   # [Project 1] JSN-SR04T x2 (S3/S5) + buzzer cảnh báo (Port mặc định: COM8)
│   │   ├── platformio.ini             # board = yolo_uno, framework = arduino
│   │   ├── build_and_flash.bat        # Script biên dịch & nạp firmware nhanh (pio run/upload/monitor)
│   │   └── src/, include/             # sensorTask/networkTask/buzzerTask (FreeRTOS trên Arduino core)
│   └── waveshare-screen/              # [Project 2] Dashboard va chạm LVGL 7" (Port mặc định: COM9)
│       ├── platformio.ini             # board = yolo_uno, framework = espidf (thuần, không Arduino)
│       ├── CMakeLists.txt, sdkconfig.defaults, partitions.csv
│       ├── build_and_flash.bat        # Script biên dịch & nạp firmware nhanh (pio run, không idf.py trực tiếp)
│       └── src/, components/          # BSP + sensor_model/coreiot_client/ui_dashboard (layout PlatformIO ESP-IDF)
├── prototypes/                        # Firmware thử nghiệm / nghiên cứu (chưa production)
│   ├── water-level-uart/              # PlatformIO – đo mực nước JSN-SR04T-V3 UART + Kalman/Median-5 filter
│   └── pulse-read-prototype/          # PlatformIO – đọc xung Trig/Echo trực tiếp GPIO (SR04M-2 Mode 3)
├── cloud/
│   └── coreiot/rule_chain/            # Cấu hình Rule-Chain CoreIoT (ThingsBoard) — tính cả field "buzzer"
├── reference/                         # Submodule mã nguồn tham khảo (vendor)
│   ├── lcd-example/                   # Waveshare ESP32-S3 Touch LCD 7 official examples
│   └── esp-faq/                       # ESP-IDF FAQ tham khảo
└── docs/
    ├── architecture/                  # Tài liệu kiến trúc & review
    └── logs/                          # Nhật ký triển khai (version logs) gộp từ mọi component
```

> **Lưu ý kiến trúc:** `sensor-node` dùng `framework = arduino` (Arduino + FreeRTOS), còn `waveshare-screen` dùng `framework = espidf` **thuần** — cả hai đều PlatformIO-hoá (`platformio.ini`, `board = yolo_uno`) nhưng lý do và giới hạn kỹ thuật của lựa chọn này (tổ hợp `arduino, espidf` không build được do bị khoá ở ESP-IDF 4.4.7 trong khi driver màn hình cần ≥5.5) được ghi chi tiết tại [docs/logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md](docs/logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md).

---

## 🛠 1. Yêu cầu Phần cứng & Phần mềm (Prerequisites)

### 🔌 Yêu cầu Phần cứng (Hardware Requirements)
1. **Bảng mạch Cảm biến Phát hiện Xe (`sensor-node`)**:
   - Vi điều khiển **ESP32-S3** (Dual-Core 240MHz, Wi-Fi/BLE).
   - **Cảm biến siêu âm chống nước JSN-SR04T** (Kết nối chân Trigger & Echo để đo khoảng cách phát hiện xe).
   - Kết nối cổng Serial/USB (Mặc định: **`COM8`**).
2. **CoreIoT Server (`app.coreiot.io`)**:
   - Nền tảng IoT ThingsBoard xử lý luồng dữ liệu thông qua **Rule-Chain**.
3. **Bảng mạch Màn hình Hiển thị Cảnh báo (`waveshare-screen`)**:
   - Màn hình cảm ứng **Waveshare ESP32-S3 Touch LCD 7** (RGB 800x480, 8MB Octal PSRAM).
   - Driver cảm ứng dung kháng GT911 (I2C Fast-mode 400kHz).
   - Controller đèn nền CH422G IO expander (I2C address `0x38`/`0x24`).
   - Kết nối cổng Serial/USB (Mặc định: **`COM9`**).

---

### 💻 Thư viện & Công cụ Cần Cài đặt (Software & Libraries)

#### A. Công cụ Môi trường Hệ thống (System Tools)
- **Hệ điều hành**: Windows 10/11 (khuyên dùng PowerShell 7 hoặc Windows PowerShell).
- **Môi trường Python Test MQTT**: Conda environment `mqtt-coreiot` tại `D:\miniconda\envs\mqtt-coreiot` (Tích hợp sẵn `paho-mqtt`).

#### B. Toolchain PlatformIO (Bắt buộc cho cả 2 firmware)
Cả `firmware/sensor-node` và `firmware/waveshare-screen` đều build bằng **PlatformIO** (`pio run -e yolo_uno`, gọi qua `build_and_flash.bat`), khác `framework` cho từng board:
- **`sensor-node`**: `framework = arduino` — PlatformIO tự tải toolchain Arduino-ESP32 tương ứng.
- **`waveshare-screen`**: `framework = espidf` **thuần** (không Arduino) — driver LCD/LVGL 9.1/GT911 cần ESP-IDF ≥5.5, PlatformIO tự tải bản ESP-IDF phù hợp (đã xác nhận build thành công với ESP-IDF 6.0.1) trong lần build đầu tiên, khác với ESP-IDF cài độc lập qua trình cài đặt Espressif (nếu có, tại `C:\Espressif\frameworks\...`, không bắt buộc cho build PlatformIO).
- Cài PlatformIO Core: `pip install platformio` hoặc dùng PlatformIO IDE/VSCode extension.

#### C. Các Thư viện Quản lý Tự động (Managed Components)
Các thư viện phụ thuộc của `waveshare-screen` được tự động tải về qua **ESP-IDF Component Manager** trong lần biên dịch đầu tiên (`src/idf_component.yml`):
- **`lvgl/lvgl`** (v9.1): Thư viện đồ họa Open-source GUI.
- **`espressif/esp_lvgl_adapter`**: Bộ adapter kết nối LVGL với ESP-IDF.
- **`espressif/esp_lcd_touch_gt911`**: Driver điều khiển cảm ứng GT911 qua bus I2C master.

Còn `sensor-node` khai báo `lib_deps` (PubSubClient, ...) trực tiếp trong `platformio.ini`, tải qua PlatformIO Library Manager.

---

## ⚡ 2. Hướng dẫn Cài đặt & Cấu hình Nhanh (Quick Start)

### Bước 1: Clone Repository (Bao gồm Submodules)
```cmd
git clone --recurse-submodules https://github.com/YdtTran/supersonic-warning-system.git
cd supersonic-sensor-ACLAB
```

### Bước 2: Khởi tạo File Cấu hình Key Local (Bảo mật)
```cmd
copy config/keys.template.json config/keys.json
```
Mở file `config/keys.json` và điền **Device Access Token** kết nối CoreIoT của bạn. File này đã được `.gitignore` bảo mật và không bị đẩy lên Git.

### Bước 3: Tự động Quét Môi trường (Cross-Device Setup)
Nếu bạn chuyển project sang máy tính mới, chạy script tự động quét vị trí cài đặt ESP-IDF và cổng COM kết nối:

```cmd
python .agents/skills/cross_device_reconfig/scripts/scan_env.py
```

---

## 🚀 3. Biên dịch & Nạp Firmware (Build & Flash)

Mỗi dự án đều tích hợp script `build_and_flash.bat` hỗ trợ biên dịch tăng tiến (Incremental Build) cực nhanh.

### A. Mô-đun Cảm biến JSN-SR04T (`firmware/sensor-node`, PlatformIO + Arduino)

```cmd
cd firmware/sensor-node

:: Biên dịch dự án (pio run -e yolo_uno)
build_and_flash.bat build

:: Biên dịch, Nạp firmware & Mở Serial Monitor (Cổng COM8)
build_and_flash.bat all COM8
```

Board này đọc 2 cảm biến S3 (`left_front`)/S5 (`right_front`), publish MQTT 2Hz, và điều khiển **buzzer vật lý** (GPIO48) cục bộ theo ngưỡng WARNING (3s/lần, <50cm)/DANGER (1s/lần, <20cm) — không qua round-trip cloud để giữ độ trễ thấp. Chi tiết: [firmware/sensor-node/README.md](firmware/sensor-node/README.md).

### B. Mô-đun Màn hình (`firmware/waveshare-screen`, PlatformIO + ESP-IDF thuần)

```cmd
cd firmware/waveshare-screen

:: Biên dịch (pio run -e yolo_uno; tự tải LVGL/ESP-IDF managed_components trong lần đầu)
build_and_flash.bat build

:: Biên dịch, Nạp firmware & Mở Serial Monitor (Cổng COM9)
build_and_flash.bat all COM9
```

Board này dùng `framework = espidf` thuần (không Arduino) do driver LCD/LVGL 9.1/GT911 yêu cầu ESP-IDF ≥5.5, trong khi tổ hợp `arduino, espidf` của PlatformIO chỉ có ESP-IDF 4.4.7. Chi tiết quyết định: [docs/logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md](docs/logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md).

---

## 🧪 4. Kiểm thử Kết nối MQTT CoreIoT bằng Python
Sử dụng script đi kèm để mô phỏng phát dữ liệu phát hiện xe lên CoreIoT server (tự động đọc token từ `config/keys.json`):

```powershell
# Gửi 1 gói tin dữ liệu khoảng cách xe mô phỏng:
& 'D:\miniconda\envs\mqtt-coreiot\python.exe' tools/test_mqtt_coreiot.py

# Gửi dữ liệu xe liên tục theo chu kỳ 2 giây:
& 'D:\miniconda\envs\mqtt-coreiot\python.exe' tools/test_mqtt_coreiot.py --loop --interval 2
```

---

## 🔍 5. Chẩn đoán Lỗi & Bắt Log Serial (Troubleshooting)

Khi nạp firmware màn hình gặp sự cố (màn hình tối đen, lỗi driver I2C), sử dụng công cụ đọc log Serial bằng Python đi kèm:

```cmd
python .agents/skills/esp32_screen_debug/scripts/read_serial.py COM9 5
```

Chi tiết chẩn đoán lỗi và giải pháp đã xử lý có thể tham khảo tại:
- Skill chẩn đoán màn hình: [.agents/skills/esp32_screen_debug/SKILL.md](file:///e:/supersonic-sensor-ACLAB/.agents/skills/esp32_screen_debug/SKILL.md)
- Nhật ký sửa lỗi màn hình: [docs/logs/WAVESHARE_SCREEN_VERSION_LOG.md](file:///e:/supersonic-sensor-ACLAB/docs/logs/WAVESHARE_SCREEN_VERSION_LOG.md)
