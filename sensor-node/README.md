# Sensor Node Module - ESP32-S3 Core Sensor Node Application

Ứng dụng vi điều khiển **ESP32-S3** phụ trách đọc và xử lý tín hiệu cảm biến siêu âm, quản lý ngoại vi (GPIO, LEDC PWM, I2C/SPI) và ghi nhật ký hệ thống trên nền tảng **ESP-IDF v6.0.2**.

---

## 🛠 Thống kê Phần cứng & Ngoại vi (Hardware Specs)

| Thành phần | Thông số kĩ thuật / Chức năng |
| :--- | :--- |
| **Microcontroller** | ESP32-S3 (Dual-Core 240MHz, Wi-Fi/BLE, 8MB Embedded PSRAM) |
| **Chân PWM (LEDC)** | **GPIO 4** (Tần số PWM: `5 kHz`, độ phân giải `13-bit`, tự động fading duty cycle) |
| **Bộ nhớ NVS** | Đã cấu hình NVS Flash để lưu trữ tham số cấu hình & trạng thái cảm biến |
| **Quản lý ngoại vi** | Tích hợp sẵn bộ driver ESP-IDF: `ledc`, `gpio`, `gptimer`, `i2c_master`, `spi_master`, `adc` |
| **Cổng Nạp Mặc định** | **`COM8`** |

---

## 📁 Cấu trúc Mô-đun (Module Structure)

```text
sensor-node/
├── CMakeLists.txt              # Cấu hình biên dịch dự án CMake
├── sdkconfig.defaults          # Cấu hình tối ưu FreeRTOS & ngoại vi ESP32-S3
├── build_and_flash.bat         # Script biên dịch tăng tiến (Incremental Build) & nạp tự động
├── README.md                   # Tài liệu hướng dẫn mô-đun cảm biến
└── main/
    ├── CMakeLists.txt          # Khai báo các thư viện phụ thuộc (driver, nvs_flash, freertos, ...)
    └── main.c                  # Luồng xử lý chính: Khởi tạo NVS, cấu hình LEDC PWM & Heartbeat Task
```

---

## 🚀 Hướng dẫn Biên dịch & Nạp Firmware (Build & Flash)

Dự án hỗ trợ script `build_and_flash.bat` tự động kích hoạt môi trường ESP-IDF v6.0.2 và biên dịch tăng tiến.

### 1. Biên dịch và Nạp tự động (Cổng mặc định COM8)
```cmd
cd sensor-node
build_and_flash.bat
```

### 2. Biên dịch, Nạp và Mở Serial Monitor
```cmd
build_and_flash.bat all COM8
```

### 3. Các thao tác lẻ khác
```cmd
build_and_flash.bat flash COM8     :: Chỉ nạp binary xuống thiết bị
build_and_flash.bat monitor COM8   :: Xem log Serial ở baudrate 115200
build_and_flash.bat clean          :: Xóa sạch thư mục build/
```

---

## 📋 Log Khởi động Mẫu (Golden Boot Log)

Log UART khi ứng dụng khởi động thành công:

```text
I (528) SENSOR_NODE: ==================================================
I (535) SENSOR_NODE:    ESP32-S3 Sensor Node Application Starting      
I (542) SENSOR_NODE: ==================================================
I (558) SENSOR_NODE: NVS Flash Initialized
I (562) SENSOR_NODE: LEDC PWM initialized on GPIO4 at 5000 Hz
I (1068) SENSOR_NODE: Heartbeat | Uptime: 1068 ms | PWM Duty: 250
I (1568) SENSOR_NODE: Heartbeat | Uptime: 1568 ms | PWM Duty: 500
```
