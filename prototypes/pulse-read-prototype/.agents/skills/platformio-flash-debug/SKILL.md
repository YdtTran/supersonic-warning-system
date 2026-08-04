---
name: platformio-flash-debug
description: Build, flash/upload firmware to hardware board using PlatformIO CLI (pio.exe) and monitor serial output to debug board behavior.
---

# PlatformIO Flash & Debug Skill

Skill hướng dẫn quy trình biên dịch (compile), nạp code (upload/flash) vào bo mạch phần cứng và theo dõi chuỗi ký tự xuất ra cổng nối tiếp (Serial Monitor) để kiểm tra, đánh giá hành vi và debug bo mạch bằng **PlatformIO CLI**.

## 📌 Cấu hình Đường dẫn PlatformIO CLI
Đường dẫn tới file thực thi `pio.exe` trên hệ thống:
```text
C:\Users\trand\.platformio\penv\Scripts\pio.exe
```

---

## 🛠️ Quy trình Thực hiện (Workflow)

### Bước 1: Biên dịch và Nạp Code (Compile & Upload)
Chạy lệnh biên dịch và nạp code lên board kết nối qua USB/Serial:

```powershell
& "C:\Users\trand\.platformio\penv\Scripts\pio.exe" run --target upload
```

> **Lưu ý:**
> - Nếu dự án có nhiều môi trường (`env`), chỉ định cụ thể môi trường bằng cờ `-e <env_name>`, ví dụ:
>   `& "C:\Users\trand\.platformio\penv\Scripts\pio.exe" run -e esp32doit-devkit-v1 --target upload`
> - Nếu chỉ muốn kiểm tra biên dịch (không nạp):
>   `& "C:\Users\trand\.platformio\penv\Scripts\pio.exe" run`

### Bước 2: Đọc Serial Output (Serial Monitor)
Đọc dữ liệu từ Serial Port phát ra từ bo mạch để theo dõi log runtime, lỗi panic hoặc thông điệp giao tiếp:

```powershell
& "C:\Users\trand\.platformio\penv\Scripts\pio.exe" device monitor
```

Hoặc chỉ định cụ thể baud rate (nếu cần):
```powershell
& "C:\Users\trand\.platformio\penv\Scripts\pio.exe" device monitor -b 115200
```

> **Mẹo:**
> - Có thể kết hợp lệnh nạp và tự động mở monitor:
>   `& "C:\Users\trand\.platformio\penv\Scripts\pio.exe" run --target upload --target monitor`

---

## 🔍 Hướng dẫn Kiểm tra Hành vi & Đưa ra Nhận xét (Debugging & Diagnostics)

Sau khi đọc Serial Output, thực hiện phân tích và nhận xét theo các bước:

1. **Kiểm tra trạng thái Khởi động (Startup Logs):**
   - Bo mạch có boot thành công không?
   - Có gặp lỗi Reset (Brownout reset - nguồn yếu, Watchdog Timeout - rò vòng lặp, Panic/Crash dump) không?

2. **Kiểm tra Chu kỳ Runtime (Application Behavior):**
   - Cấu hình `Serial.begin(baudrate)` trong `setup()` có khớp với baudrate của monitor không?
   - Dữ liệu in ra có đúng logic mong đợi hay xuất hiện ký tự rác (garbled text)?

3. **Chẩn đoán Lỗi & Đóng góp ý kiến (Analysis & Feedback):**
   - Nêu rõ tình trạng bo mạch (Nạp thành công, chạy bình thường / Crash rò bộ nhớ / Lỗi cổng Serial).
   - Đưa ra nhận xét chi tiết và hướng khắc phục cụ thể nếu có bất thường.
