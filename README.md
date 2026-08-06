# Vehicle Detection & Warning System using JSN-SR04T Supersonic Sensor & ESP-NOW (ACLAB)

Hệ thống nhúng vi điều khiển **ESP32-S3** phát hiện xe thông qua **cảm biến siêu âm chống nước JSN-SR04T**. Trên nhánh hiện tại, khoảng cách đã lọc được gửi **trực tiếp qua ESP-NOW** (đường truyền Wi-Fi cục bộ, không qua cloud) từ mô-đun cảm biến (`sensor-node`) tới mô-đun màn hình cảm ứng **Waveshare ESP32-S3 Touch LCD 7 inch** (`waveshare-screen`), nơi mức cảnh báo được tự đánh giá cục bộ và hiển thị trên giao diện đồ họa LVGL. Đường **CoreIoT (ThingsBoard) + Rule-Chain** của kiến trúc trước đó vẫn còn trong mã nguồn (không bị xoá) nhưng **hiện không hoạt động** — xem [`docs/architecture/ESPNOW_NETWORK.md`](docs/architecture/ESPNOW_NETWORK.md).

---

## 🔄 Luồng Dữ liệu & Kiến trúc Hệ thống (Data Flow & Architecture)

```text
 ┌────────────────────────┐
 │   Cảm biến JSN-SR04T   │ (Cảm biến siêu âm chống nước đo khoảng cách xe, x3: S1/S3/S5)
 └───────────┬────────────┘
             │ (Echo / Trig GPIO)
             ▼
 ┌────────────────────────┐
 │  ESP32-S3 Sensor Node  │ (firmware/sensor-node: đo/lọc khoảng cách, buzzer cục bộ)
 └───────────┬────────────┘
             │ (ESP-NOW trực tiếp, channel cố định, không qua Wi-Fi AP/cloud)
             ▼
 ┌────────────────────────┐
 │ Waveshare Screen Node  │ (firmware/waveshare-screen: tự đánh giá hazard cục bộ, hiển thị LCD 7")
 └────────────────────────┘

 ┌ (không hoạt động trên nhánh này, giữ lại trong mã nguồn) ─────────────┐
 │ ESP32-S3 Sensor Node → Wi-Fi MQTT → CoreIoT (app.coreiot.io)         │
 │   Rule-Chain → MQTT Shared Attributes → Waveshare Screen Node        │
 └────────────────────────────────────────────────────────────────────┘
```

---

## 📐 Kiến trúc Dự án (Repository Architecture)

```text
supersonic-sensor-ACLAB/
├── AGENTS.template.md                 # Template cấu hình môi trường & quy tắc cho AI Agent (Cross-Device)
├── AGENTS.md                          # Cấu hình môi trường local & cổng COM (Ignored by Git)
├── CONTRIBUTING.md                    # Quy trình Git, quy ước commit, quy tắc code, ghi log triển khai
├── SECURITY.md                        # Vấn đề bảo mật đã biết & cách báo lỗi
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
│   ├── sensor-node/                   # [Project 1] JSN-SR04T x3 (S1/S3/S5) + buzzer cảnh báo, gửi ESP-NOW (Port mặc định: COM8)
│   │   ├── platformio.ini             # board = yolo_uno, framework = arduino
│   │   ├── build_and_flash.bat        # Script biên dịch & nạp firmware nhanh (pio run/upload/monitor)
│   │   └── src/, include/             # sensorTask/networkTask/buzzerTask (FreeRTOS trên Arduino core)
│   └── waveshare-screen/              # [Project 2] Dashboard va chạm LVGL 7" (Port mặc định: COM9)
│       ├── platformio.ini             # board = yolo_uno, framework = espidf (thuần, không Arduino)
│       ├── CMakeLists.txt, sdkconfig.defaults, partitions.csv
│       ├── build_and_flash.bat        # Script biên dịch & nạp firmware nhanh (pio run, không idf.py trực tiếp)
│       └── src/, components/          # BSP + sensor_model/ui_dashboard, nhận ESP-NOW trong src/main.c (coreiot_client giữ lại, không dùng)
├── prototypes/                        # Firmware thử nghiệm / nghiên cứu (chưa production)
│   ├── water-level-uart/              # PlatformIO – đo mực nước JSN-SR04T-V3 UART + Kalman/Median-5 filter
│   └── pulse-read-prototype/          # PlatformIO – đọc xung Trig/Echo trực tiếp GPIO (SR04M-2 Mode 3)
├── cloud/
│   └── coreiot/rule_chain/            # Cấu hình Rule-Chain CoreIoT (ThingsBoard) — tính cả field "buzzer"
├── reference/                         # Submodule mã nguồn tham khảo (vendor)
│   ├── lcd-example/                   # Waveshare ESP32-S3 Touch LCD 7 official examples
│   └── esp-faq/                       # ESP-IDF FAQ tham khảo
├── report/                            # Báo cáo kỹ thuật (README + LaTeX/PDF)
└── docs/
    ├── README.md                      # Mục lục điều hướng toàn bộ tài liệu trong docs/
    ├── API_GUIDE.md                   # Hướng dẫn API thư viện/component + cấu hình phần mềm
    ├── architecture/                  # Tài liệu kiến trúc & review
    └── logs/                          # Nhật ký triển khai (version logs) gộp từ mọi component
```

---

## 📚 Tài liệu Module (Module Documentation)

| Module | Tài liệu |
| --- | --- |
| **Mục lục toàn bộ tài liệu** | [`docs/README.md`](docs/README.md) — điểm vào cho `docs/architecture/`, `docs/logs/`, khoảng trống tài liệu đã biết |
| Toàn hệ thống | [`report/README.md`](report/README.md) — báo cáo kỹ thuật đầy đủ (kiến trúc, phần cứng, nhật ký phát triển, hạn chế); bản trang trọng: [`report/report.pdf`](report/report.pdf) |
| **ESP-NOW (đường đang dùng)** | [`docs/architecture/ESPNOW_NETWORK.md`](docs/architecture/ESPNOW_NETWORK.md) — schema `espnow_sensor_msg_t`, MAC/channel dùng chung, cách đồng bộ khi đổi board |
| **API & cấu hình phần mềm** | [`docs/API_GUIDE.md`](docs/API_GUIDE.md) — API từng thư viện/component (`UltrasonicSensor`, `DistanceFilter`, `EspNowClient`, `sensor_model`, `ui_dashboard`; `CoreiotClient`/`coreiot_client`/Rule-Chain không dùng trên nhánh này), ví dụ code, cách đổi ngưỡng cảnh báo |
| Đóng góp | [`CONTRIBUTING.md`](CONTRIBUTING.md) — quy trình Git, quy ước commit, quy tắc code, ghi log triển khai |
| Bảo mật | [`SECURITY.md`](SECURITY.md) — vấn đề bảo mật đã biết (token hardcode, MQTT không TLS), cách báo lỗi |
| Lịch sử thay đổi | [`CHANGELOG.md`](CHANGELOG.md) — tóm tắt thay đổi theo mốc thời gian (Keep a Changelog, chưa có SemVer/tag release) |
| Quyết định kiến trúc | [`docs/adr/`](docs/adr/) — Architecture Decision Records (PlatformIO thống nhất, ESP-IDF thuần cho `waveshare-screen`, CoreIoT lịch sử, bộ lọc Cluster+EMA) |
| Schema dữ liệu | [`docs/architecture/DATA_SCHEMA.md`](docs/architecture/DATA_SCHEMA.md) — schema payload ESP-NOW `sensor-node` ↔ `waveshare-screen` và struct `sensor_model` (đường MQTT/CoreIoT cũ ghi chú là không hoạt động) |
| `firmware/sensor-node` | [`firmware/sensor-node/README.md`](firmware/sensor-node/README.md) — đo/lọc 3 cảm biến S1/S3/S5, buzzer cục bộ, gửi ESP-NOW |
| `firmware/waveshare-screen` | [`firmware/waveshare-screen/README.md`](firmware/waveshare-screen/README.md) — dashboard LVGL 800×480, BSP màn hình/cảm ứng, build & flash, debug serial |
| `cloud/coreiot/rule_chain/` (không dùng trên nhánh này) | [`cloud/coreiot/rule_chain/supersonic_rule_chain.json`](cloud/coreiot/rule_chain/supersonic_rule_chain.json) — chi tiết từng node & cách sửa ngưỡng: [`docs/API_GUIDE.md` mục 4](docs/API_GUIDE.md#4-rule-chain-coreiot--cấu-hình--api-node) |
| `prototypes/pulse-read-prototype` | Đọc trực tiếp Trig/Echo GPIO (SR04M-2 Mode 3) — xem [`report/README.md` mục 8](report/README.md#8-prototype-thử-nghiệm) |
| `prototypes/water-level-uart` | Đo mực nước JSN-SR04T-V3 UART + Kalman/Median-5 — mã nguồn tại [`prototypes/water-level-uart/`](prototypes/water-level-uart/) |
| Nhật ký phát triển | [`docs/logs/`](docs/logs/) — theo từng chủ đề (UART GPIO43, SR04M2 driver, dashboard library, session log màn hình, …) |
| Kiến trúc & review | [`docs/architecture/`](docs/architecture/) — checkpoint tiến độ, review kiến trúc LVGL demos, review ví dụ Waveshare |

> **Lưu ý kiến trúc:** `sensor-node` dùng `framework = arduino` (Arduino + FreeRTOS), còn `waveshare-screen` dùng `framework = espidf` **thuần** — cả hai đều PlatformIO-hoá (`platformio.ini`, `board = yolo_uno`) nhưng lý do và giới hạn kỹ thuật của lựa chọn này (tổ hợp `arduino, espidf` không build được do bị khoá ở ESP-IDF 4.4.7 trong khi driver màn hình cần ≥5.5) được ghi chi tiết tại [docs/logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md](docs/logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md).

---

## 🛠 1. Yêu cầu Phần cứng & Phần mềm (Prerequisites)

### 🔌 Yêu cầu Phần cứng (Hardware Requirements)
1. **Bảng mạch Cảm biến Phát hiện Xe (`sensor-node`)**:
   - Vi điều khiển **ESP32-S3** (Dual-Core 240MHz, Wi-Fi/BLE).
   - **Cảm biến siêu âm chống nước JSN-SR04T** (Kết nối chân Trigger & Echo để đo khoảng cách phát hiện xe).
   - Kết nối cổng Serial/USB (Mặc định: **`COM8`**).
2. **CoreIoT Server (`app.coreiot.io`)** — *(không bắt buộc trên nhánh hiện tại, xem lưu ý ở mục "Luồng Dữ liệu" phía trên)*:
   - Nền tảng IoT ThingsBoard xử lý luồng dữ liệu thông qua **Rule-Chain**, dùng khi khôi phục lại đường MQTT/CoreIoT.
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

Board này đọc 3 cảm biến S1 (`front`)/S3 (`left_front`)/S5 (`right_front`), gửi khoảng cách qua **ESP-NOW** trực tiếp tới `waveshare-screen` 2Hz (không qua Wi-Fi AP/MQTT/cloud), và điều khiển **buzzer vật lý** (GPIO48) cục bộ theo ngưỡng WARNING (3s/lần, <50cm)/DANGER (1s/lần, <20cm) — hoàn toàn độc lập với ESP-NOW để giữ độ trễ thấp. Chi tiết: [firmware/sensor-node/README.md](firmware/sensor-node/README.md).

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

## 🧪 4. Kiểm thử Kết nối MQTT CoreIoT bằng Python (không dùng trên nhánh hiện tại)

> Script này kiểm thử đường MQTT/CoreIoT cũ — trên nhánh hiện tại, `sensor-node` không publish MQTT nữa (xem mục "Luồng Dữ liệu" ở đầu file), nên script chỉ hữu ích khi làm việc với đường CoreIoT được khôi phục lại.

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
