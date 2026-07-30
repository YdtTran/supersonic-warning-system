# Hardware UART 1 on GPIO 44 Implementation Log

## 🎯 Mục Tiêu Công Việc
Chuyển đổi ứng dụng đọc cảm biến siêu âm JSN-SR04T từ **Software UART** (quét bit bằng phần mềm) sang **Hardware UART1 vật lý** của ESP32-S3 trên chân **GPIO 44** nhằm khắc phục lỗi trượt timing bit và tăng độ ổn định khi đo khoảng cách xa (>50cm).

---

## 🌿 Thông Tin Git Branch
- **Branch Name:** `feature/hardware-uart-gpio44`
- **Commit:** `feat(supersonic-test): switch to Hardware UART1 on GPIO 44 for JSN-SR04T`
- **Remote Status:** Đã push thành công lên `origin/feature/hardware-uart-gpio44`.

---

## 🛠️ Chi Tiết Thay Đổi Mã Nguồn
- **`supersonic-test/src/main.cpp`**:
  - Loại bỏ `SoftUART.h`.
  - Khởi tạo **HardwareSerial SensorSerial(1)** sử dụng **UART1 phần cứng** của ESP32-S3.
  - Cấu hình chân **RX = GPIO 44**, Baud rate `9600`.
  - Sử dụng cơ chế đọc buffer phần cứng FIFO (`available()`, `peek()`, `readBytes()`) và đồng bộ khung truyền 4-byte (`0xFF + Data_H + Data_L + Checksum`).
  - Duy trì kiến trúc **Dual Core FreeRTOS**: Task đọc Hardware UART trên **Core 1**, Task hiển thị trên **Core 0** qua `xSensorQueue`.

---

## ✅ Kết Quả Kiểm Thử (Build Verification)
- **Command:** `pio run -d supersonic-test`
- **Status:** **SUCCESS**
- **Memory Usage:**
  - RAM: `5.8%` (used 19,096 bytes / 327,680 bytes)
  - Flash: `8.5%` (used 282,517 bytes / 3,342,336 bytes)

---

## 🚀 Hướng Dẫn Vận Hành / Flash Board
```cmd
# Chuyển sang branch feature/hardware-uart-gpio44 (nếu chưa ở branch này):
git checkout feature/hardware-uart-gpio44

# Flash firmware lên ESP32-S3:
pio run -d supersonic-test --target upload

# Mở Serial Monitor xem log:
pio device monitor -b 115200

# Chạy phần mềm đồ thị đường khoảng cách thời gian thực (Python Plotter):
& 'D:\miniconda\envs\mqtt-coreiot\python.exe' tools/plot_ultrasonic_distance.py --port COM3
```

