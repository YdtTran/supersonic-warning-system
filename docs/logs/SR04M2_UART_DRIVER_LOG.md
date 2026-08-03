# SR04M-2 Ultrasonic Sensor Driver & Diagnosis Log

## 📌 Phân Tích & Nguyên Nhân Kỹ Thuật (Root Cause Analysis)

Qua quá trình kiểm thử tự động, hệ thống đã phát hiện 2 nguyên nhân cốt lõi khiến việc đọc UART SR04M-2 trên chân GPIO 43 bị thất bại:

### 1. Trùng Bus UART0 Cứng trên ESP32-S3 (Pin Contention)
- Trên chip **ESP32-S3**, cặp chân **GPIO 43 (U0TXD)** và **GPIO 44 (U0RXD)** là cổng **UART0 mặc định** phần cứng của vi xử lý.
- Khi bật `Serial.begin()` hoặc bootloader chạy, ESP32-S3 liên tục xuất dữ liệu Console Log ra chân GPIO 43.
- Nối chân TX của SR04M-2 trực tiếp vào GPIO 43 dẫn tới xung đột 2 nguồn phát (ESP32-S3 UART0 TX và SR04M-2 TX đấu đầu nhau), làm méo tín hiệu điện áp tạo ra luồng byte rác `0x00`, `0xE0`, `0xF0`.
- **Khuyến nghị kết nối**: Đổi sang cặp chân GPIO độc lập như **RX = GPIO 18, TX = GPIO 17** (hoặc **RX = GPIO 16, TX = GPIO 15**).

### 2. Chế độ Xuất Dữ liệu Mặc định từ Nhà Máy (Mode 1: Trig/Echo Pulse)
- Mạch cảm biến **SR04M-2 / AJ-SR04M-2.0 / JSN-SR04T-2.0** nguyên bản xuất xưởng (khi vị trí **R27** trên lưng mạch cảm biến chưa được hàn) mặc định hoạt động ở **Chế độ 1: Xung GPIO Trig/Echo** (chân Trig nhận xung 10µs, chân Echo trả về độ rộng xung HIGH).
- Kết quả test thực tế đã đo được xung phản hồi và xuất khoảng cách: `Khoảng cách: 27 mm (2.7 cm)`.
- **Để mạch chuyển sang chế độ UART xuất gói tin 4 bytes `0xFF + DataH + DataL + Checksum`**:
  - Hàn điện trở **47kΩ** vào vị trí **R27** $\rightarrow$ Mode 2 (UART Tự Động).
  - Hàn điện trở **120kΩ** vào vị trí **R27** $\rightarrow$ Mode 3 (UART Trigger `0x55`).

---

## 🛠️ Cấu Hình Phần Cứng Khuyên Dùng

| Chân SR04M-2 | Chân ESP32-S3 | Ghi chú |
| :--- | :--- | :--- |
| **VCC** | **5V** | Nguồn 5V bắt buộc cho cảm biến siêu âm |
| **GND** | **GND** | Nối đất chung |
| **TX / Echo** | **GPIO 18 (RX)** | Chân nhận dữ liệu / xung Echo (tránh trùng UART0) |
| **RX / Trig** | **GPIO 17 (TX)** | Chân phát lệnh kích xung / Trigger (tránh trùng UART0) |

---

## 💻 Mã Nguồn Đã Cập Nhật Auto-Detect (main.cpp)

File [main.cpp](file:///e:/supersonic-sensor-ACLAB/supersonic-test/src/main.cpp) đã được cập nhật tính năng **tự động chuyển đổi giữa chế độ UART và chế độ Xung Trig/Echo**:
- Nếu cảm biến được hàn R27 (UART Mode): Tự động giải mã gói tin `0xFF` 9600 baud.
- Nếu cảm biến nguyên bản (Trig/Echo Mode): Tự động phát xung 10µs và đo thời gian `pulseIn`.

Firmware đã nạp thành công lên ESP32-S3.
