# Water Level Measurement Log - Median 5 Filter

## 🎯 Mục Tiêu Công Việc
Áp dụng **Kịch bản 1 (Đo Mực Nước Bể / Tháp Nước)** và tích hợp thuật toán **Lọc Trung Vị 5 Mẫu (Median-5 Filter)** trên ESP32-S3 nhằm triệt tiêu hoàn toàn các điểm nảy rác (outliers/spikes) do sóng dội mặt nước và góc phản xạ nghiêng gây ra.

---

## 🌿 Thông Tin Git Branch
- **Branch Name:** `feature/hardware-uart-gpio44`
- **Chân Hardware UART RX:** `GPIO 43`
- **Commit:** `feat(supersonic-test): implement Median-5 filter for water level measurement`

---

## 🛠️ Nguyên Lý Hoạt Động Thuật Toán Lọc Trung Vị (Median-5 Filter)
1. **Thu thập dữ liệu:** `TaskReadSensorHardwareUART` liên tục đọc 5 mẫu dữ liệu khoảng cách hợp lệ từ Hardware UART1 (chu kỳ đọc giữa các mẫu $150\text{ ms}$).
2. **Sắp xếp & Chọn Trung vị:**
   - Dùng thuật toán Bubble Sort sắp xếp dãy 5 phần tử theo thứ tự tăng dần: $S = \{s_0, s_1, s_2, s_3, s_4\}$.
   - Giá trị khoảng cách đầu ra đại diện là phần tử ở vị trí chính giữa $s_2$.
3. **Đưa vào FreeRTOS Queue:** Đẩy kết quả đã được làm mịn vào `xSensorQueue` sang Core 0 hiển thị/truyền dẫn với tần số cập nhật khoảng **1 Hz**.

---

## ✅ Kết Quả Kiểm Thử (Build Verification)
- **Command:** `pio run -d supersonic-test`
- **Status:** **SUCCESS**
- **Memory Usage:**
  - RAM: `5.8%` (used 19,096 bytes / 327,680 bytes)
  - Flash: `8.5%` (used 282,685 bytes / 3,342,336 bytes)

---

## 🚀 Hướng Dẫn Chạy Mô Phỏng / Nạp Firmware
```cmd
# Build và Nạp firmware lên ESP32-S3:
pio run -d supersonic-test --target upload

# Chạy ứng dụng Đồ thị đường thời gian thực:
& 'D:\miniconda\envs\mqtt-coreiot\python.exe' tools/plot_ultrasonic_distance.py --port COM16
```
