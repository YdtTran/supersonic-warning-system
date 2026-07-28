# Supersonic Sensor & LVGL Display System (ACLAB)

Hệ thống nhúng vi điều khiển **ESP32-S3** bao gồm 2 dự án ESP-IDF độc lập: mô-đun cảm biến siêu âm (`sensor-node`) và ứng dụng đồ họa hiển thị cảm ứng màn hình 7 inch (`waveshare-screen`).

---

## 📐 Kiến trúc Dự án (Repository Architecture)

```text
supersonic-sensor-ACLAB/
├── AGENTS.template.md                 # Template cấu hình môi trường cho máy mới (Cross-Device)
├── AGENTS.md                          # Cấu hình môi trường & cổng COM máy local (Ignored by Git)
├── .gitignore                         # Cấu hình bỏ qua build artifacts & file cá nhân
├── .agents/                           # Thư mục chứa Custom Skills cho AI Agents
│   └── skills/
│       ├── cross_device_reconfig/     # Skill tự động quét & cấu hình project trên máy mới
│       └── esp32_screen_debug/        # Skill chẩn đoán lỗi hiển thị, I2C driver & trích xuất log
├── sensor-node/                       # [Project 1] Firmware vi điều khiển cảm biến (Port mặc định: COM8)
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   ├── build_and_flash.bat            # Script biên dịch & nạp firmware nhanh
│   └── main/                          # Mã nguồn chính & driver ngoại vi
└── waveshare-screen/                  # [Project 2] Firmware màn hình cảm ứng LVGL 7" (Port mặc định: COM9)
    ├── CMakeLists.txt
    ├── sdkconfig.defaults
    ├── build_and_flash.bat            # Script biên dịch & nạp firmware nhanh
    └── main/                          # Mã nguồn BSP (Display/Touch) & UI (LVGL)
```

---

## 🛠 1. Yêu cầu Phần cứng & Phần mềm (Prerequisites)

### 🔌 Yêu cầu Phần cứng (Hardware Requirements)
1. **Bảng mạch Cảm biến (`sensor-node`)**:
   - Vi điều khiển **ESP32-S3** (Dual-Core 240MHz, Wi-Fi/BLE).
   - Cảm biến đo khoảng cách siêu âm (Supersonic distance sensor) và các ngoại vi GPIO/PWM.
   - Kết nối cổng Serial/USB (Mặc định: **`COM8`**).
2. **Bảng mạch Màn hình (`waveshare-screen`)**:
   - Màn hình cảm ứng **Waveshare ESP32-S3 Touch LCD 7** (Độ phân giải RGB 800x480, 8MB Octal PSRAM).
   - Driver cảm ứng dung kháng GT911 (I2C Fast-mode 400kHz).
   - Controller đèn nền CH422G IO expander (I2C address `0x38`/`0x24`).
   - Kết nối cổng Serial/USB (Mặc định: **`COM9`**).

---

### 💻 Thư viện & Công cụ Cần Cài đặt (Software & Libraries)

#### A. Công cụ Môi trường Hệ thống (System Tools)
- **Hệ điều hành**: Windows 10/11 (khuyên dùng PowerShell 7 hoặc Windows PowerShell có sẵn).
- **Python**: Python 3.10+ (Đã cài đặt sẵn các thư viện chuẩn: `os`, `sys`, `subprocess`, `re`, `glob`).

#### B. Toolchain ESP-IDF (Bắt buộc)
Dự án được xây dựng và kiểm thử trên **ESP-IDF v6.0.2** (hoặc v5.3+):
- **ESP-IDF Framework**: Cài đặt tại `C:\Espressif\frameworks\esp-idf-v6.0.2` hoặc `E:\esp\v6.0.2\esp-idf`.
- **ESP-IDF Tools Path**: `C:\Espressif\tools`.
- **Trình biên dịch & Build System**: `xtensa-esp32s3-elf-gcc`, `CMake`, `Ninja` (được tự động cấu hình qua ESP-IDF Installer).
- **PowerShell Profile**: `C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1`.

#### C. Các Thư viện Quản lý Tự động (Managed Components)
Các thư viện phụ thuộc của dự án được tự động tải về qua **ESP-IDF Component Manager** trong lần biên dịch đầu tiên (khai báo tại `main/idf_component.yml`):
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

### Bước 2: Tự động Quét Môi trường & Thiết lập (Cross-Device Setup)
Nếu bạn chuyển project sang máy tính mới, chạy script tự động quét vị trí cài đặt ESP-IDF và cổng COM kết nối:

```cmd
python .agents/skills/cross_device_reconfig/scripts/scan_env.py
```

> **Cách thức hoạt động:** Script sẽ tự động tìm kiếm `IDF_PATH`, `Python venv`, profile PowerShell và các cổng COM kết nối, sau đó tạo ra file **`AGENTS.md`** local dựa trên **`AGENTS.template.md`**.

---

## 🚀 3. Biên dịch & Nạp Firmware (Build & Flash)

Mỗi dự án đều tích hợp script `build_and_flash.bat` hỗ trợ biên dịch tăng tiến (Incremental Build) cực nhanh.

### A. Mô-đun Cảm biến (`sensor-node`)
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

## 🔍 4. Chẩn đoán Lỗi & Bắt Log Serial (Troubleshooting)

Khi nạp firmware màn hình gặp sự cố (màn hình tối đen, lỗi driver I2C), sử dụng công cụ đọc log Serial bằng Python đi kèm:

```cmd
python .agents/skills/esp32_screen_debug/scripts/read_serial.py COM9 5
```

Chi tiết chẩn đoán lỗi và giải pháp đã xử lý có thể tham khảo tại:
- Skill chẩn đoán màn hình: [.agents/skills/esp32_screen_debug/SKILL.md](file:///e:/supersonic-sensor-ACLAB/.agents/skills/esp32_screen_debug/SKILL.md)
- Nhật ký sửa lỗi màn hình: [waveshare-screen/version_logs/VERSION_LOG.md](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/version_logs/VERSION_LOG.md)
