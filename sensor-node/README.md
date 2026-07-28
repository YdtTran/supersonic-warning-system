# Sensor Node Module - ESP32-S3 JSN-SR04T Vehicle Detection Application

Mô-đun vi điều khiển **ESP32-S3** phụ trách đọc tín hiệu từ **cảm biến siêu âm chống nước JSN-SR04T** để đo khoảng cách và phát hiện sự xuất hiện của xe. Dữ liệu xe phát hiện (khoảng cách, trạng thái cảnh báo) sẽ được đẩy lên **CoreIoT (ThingsBoard)** server qua giao thức Wi-Fi MQTT Telemetry.

---

## 🛠 Thống kê Phần cứng & Ngoại vi (Hardware Specs)

| Thành phần | Thông số kĩ thuật / Chức năng |
| :--- | :--- |
| **Microcontroller** | ESP32-S3 (Dual-Core 240MHz, Wi-Fi/BLE, 8MB Embedded PSRAM) |
| **Cảm biến Siêu âm** | **JSN-SR04T** (Cảm biến siêu âm chống nước đo khoảng cách phát hiện xe) |
| **Giao tiếp Cảm biến** | Chân **Trigger** (Phát xung kích hoạt) & **Echo** (Đo thời gian phản hồi) qua GPIO |
| **Chân PWM (LEDC)** | **GPIO 4** (Tần số PWM: `5 kHz`, độ phân giải `13-bit`, tín hiệu cảnh báo/fading) |
| **Bộ nhớ NVS** | Đã cấu hình NVS Flash để lưu trữ tham số cấu hình & trạng thái cảm biến |
| **MQTT Telemetry** | Gửi thông số khoảng cách xe (`distance_cm`) lên Topic `v1/devices/me/telemetry` trên CoreIoT |
| **Cổng Nạp Mặc định** | **`COM8`** |

---

## 📁 Cấu trúc Mô-đun (Module Structure)

```text
sensor-node/
├── CMakeLists.txt              # Cấu hình biên dịch dự án CMake
├── sdkconfig.defaults          # Cấu hình tối ưu FreeRTOS & ngoại vi ESP32-S3
├── build_and_flash.bat         # Script biên dịch tăng tiến (Incremental Build) & nạp tự động
├── README.md                   # Tài liệu hướng dẫn mô-đun cảm biến JSN-SR04T
└── main/
    ├── CMakeLists.txt          # Khai báo các thư viện phụ thuộc (driver, nvs_flash, freertos, ...)
    └── main.c                  # Đọc cảm biến JSN-SR04T, xử lý tín hiệu xe & phát dữ liệu MQTT
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

Log UART khi ứng dụng cảm biến JSN-SR04T khởi động thành công:

```text
I (528) SENSOR_NODE: ==================================================
I (535) SENSOR_NODE:    ESP32-S3 JSN-SR04T Vehicle Detection Node      
I (542) SENSOR_NODE: ==================================================
I (558) SENSOR_NODE: NVS Flash Initialized
I (562) SENSOR_NODE: JSN-SR04T Sensor initialized on Trig/Echo pins
I (1068) SENSOR_NODE: Vehicle Detection | Distance: 45.2 cm | Status: WARNING
```
