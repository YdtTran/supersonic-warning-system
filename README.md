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
 │  ESP32-S3 Sensor Node  │ (sensor-node: Xử lý khoảng cách, phát hiện xe)
 └───────────┬────────────┘
             │ (Wi-Fi MQTT Telemetry: v1/devices/me/telemetry)
             ▼
 ┌────────────────────────┐
 │  CoreIoT Cloud Server  │ (app.coreiot.io: Xử lý ngưỡng qua Rule-Chain)
 └───────────┬────────────┘
             │ (MQTT Subscribe / Rule-Chain Forward)
             ▼
 ┌────────────────────────┐
 │ Waveshare Screen Node  │ (waveshare-screen: Hiển thị cảnh báo trực quan trên LCD 7")
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
│       └── esp32_screen_debug/        # Skill chẩn đoán lỗi hiển thị, I2C driver & trích xuất log
├── tools/                             # Công cụ kiểm thử MQTT với môi trường Conda 'mqtt-coreiot'
│   └── test_mqtt_coreiot.py           # Script mô phỏng phát dữ liệu xe & kiểm tra kết nối CoreIoT
├── sensor-node/                       # [Project 1] Firmware cảm biến JSN-SR04T (Port mặc định: COM8)
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   ├── build_and_flash.bat            # Script biên dịch & nạp firmware nhanh
│   └── main/                          # Mã nguồn đọc JSN-SR04T & phát dữ liệu xe lên CoreIoT
└── waveshare-screen/                  # [Project 2] Firmware màn hình cảm ứng LVGL 7" (Port mặc định: COM9)
    ├── CMakeLists.txt
    ├── sdkconfig.defaults
    ├── build_and_flash.bat            # Script biên dịch & nạp firmware nhanh
    └── main/                          # Mã nguồn BSP & Giao diện LVGL nhận dữ liệu từ CoreIoT Rule-Chain
```

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

#### B. Toolchain ESP-IDF (Bắt buộc)
Dự án được xây dựng và kiểm thử trên **ESP-IDF v6.0.2** (hoặc v5.3+):
- **ESP-IDF Framework**: Cài đặt tại `C:\Espressif\frameworks\esp-idf-v6.0.2` hoặc `E:\esp\v6.0.2\esp-idf`.
- **ESP-IDF Tools Path**: `C:\Espressif\tools`.
- **Trình biên dịch & Build System**: `xtensa-esp32s3-elf-gcc`, `CMake`, `Ninja` (được tự động cấu hình qua ESP-IDF Installer).
- **PowerShell Profile**: `C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1`.

#### C. Các Thư viện Quản lý Tự động (Managed Components)
Các thư viện phụ thuộc của dự án được tự động tải về qua **ESP-IDF Component Manager** trong lần biên dịch đầu tiên (`main/idf_component.yml`):
- **`lvgl/lvgl`** (v8.x/v9.x): Thư viện đồ họa Open-source GUI.
- **`espressif/esp_lvgl_adapter`**: Bộ adapter kết nối LVGL với ESP-IDF.
- **`espressif/esp_lcd_touch_gt911`**: Driver điều khiển cảm ứng GT911 qua bus I2C master.

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

### A. Mô-đun Cảm biến JSN-SR04T (`sensor-node`)
```cmd
cd sensor-node

:: Biên dịch dự án
build_and_flash.bat build

:: Biên dịch, Nạp firmware & Mở Serial Monitor (Cổng COM8)
build_and_flash.bat all COM8
```

### B. Mô-đun Màn hình (`waveshare-screen`)
```cmd
cd waveshare-screen

:: Biên dịch (tự động tải LVGL trong lần đầu)
build_and_flash.bat build

:: Biên dịch, Nạp firmware & Mở Serial Monitor (Cổng COM9)
build_and_flash.bat all COM9
```

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
- Nhật ký sửa lỗi màn hình: [waveshare-screen/version_logs/VERSION_LOG.md](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/version_logs/VERSION_LOG.md)
