# 0001. PlatformIO thống nhất cho cả 2 firmware dù khác framework

**Trạng thái**: Accepted
**Ngày**: giai đoạn ~2026-08-03/04 (đợt tái cấu trúc `waveshare-screen` sang layout PlatformIO, xem [0002](0002-espidf-pure-for-waveshare-screen.md))

## Bối cảnh (Context)

Dự án có 2 firmware ESP32-S3 khác mục đích và khác framework runtime: `firmware/sensor-node` (đo/lọc cảm biến, publish MQTT, dùng Arduino core) và `firmware/waveshare-screen` (dashboard LVGL, ban đầu là project ESP-IDF thuần build bằng `idf.py`/CMake trực tiếp, không qua PlatformIO). Hai project dùng 2 công cụ build khác nhau (`idf.py` vs PlatformIO), 2 quy ước cấu trúc thư mục khác nhau (`main/` kiểu ESP-IDF vs `src/`/`include/` kiểu PlatformIO), gây khó khăn khi làm việc song song trên cả 2 board: không có toolchain thống nhất, không thể dùng chung script build/flash, người mới phải học 2 quy trình khác nhau.

## Quyết định (Decision)

Đưa cả 2 firmware về cùng layout PlatformIO (`src/`, `include/`, `boards/yolo_uno.json`, `platformio.ini`, `build_and_flash.bat`), build bằng cùng một lệnh `pio run -e yolo_uno` — nhưng **giữ khác `framework`** theo đúng nhu cầu kỹ thuật của từng board: `sensor-node` dùng `framework = arduino`, `waveshare-screen` dùng `framework = espidf` thuần (không Arduino, lý do kỹ thuật chi tiết xem [ADR 0002](0002-espidf-pure-for-waveshare-screen.md)). PlatformIO đóng vai trò lớp toolchain/build-system chung, không ép cả 2 project phải dùng chung Arduino core.

## Hệ quả (Consequences)

**Tích cực:**
- Một toolchain (`pio`), một bộ lệnh (`build`/`upload`/`monitor`), một quy ước thư mục cho cả 2 project — giảm chi phí chuyển ngữ cảnh khi làm việc trên cả 2 board.
- `build_and_flash.bat` giống hệt UX ở cả 2 firmware (`build`/`flash`/`monitor`/`all`/`clean`), dù bên dưới `waveshare-screen` gọi `pio run` thay vì `idf.py` trực tiếp.
- Không cần cài đặt/đồng bộ ESP-IDF độc lập ngoài PlatformIO (dù vẫn có thể trỏ tới bản ESP-IDF cài riêng nếu cần, không bắt buộc).

**Đánh đổi:**
- PlatformIO's ESP-IDF builder (`espidf.py`) đòi hỏi cứng một thư mục `src/` ở gốc project bất kể có dùng Arduino hay không — buộc phải đổi tên `main/` (quy ước gốc của ESP-IDF) thành `src/` trên `waveshare-screen`, tạo khác biệt nhỏ so với project ESP-IDF thuần build bằng `idf.py` bên ngoài PlatformIO.
- Vẫn phải quản lý 2 bộ `lib_deps`/managed-components riêng biệt (Arduino Library Manager cho `sensor-node`, ESP-IDF Component Manager qua `idf_component.yml` cho `waveshare-screen`) — PlatformIO không thống nhất được lớp phụ thuộc, chỉ thống nhất lớp lệnh build.
- Lần build đầu của `waveshare-screen` vẫn tốn thời gian tải toolchain + ESP-IDF + managed components (~545s, ~264MB+) tương đương build ESP-IDF thuần — PlatformIO không làm nhanh hơn, chỉ làm đồng nhất cách gọi.

## Tham khảo

- [`report/README.md` mục 4](../../report/README.md#4-công-cụ--framework) — bảng lựa chọn công cụ & lý do.
- [`docs/logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md`](../logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md) — phát hiện kỹ thuật về yêu cầu thư mục `src/` của PlatformIO's ESP-IDF builder.
- [`README.md` gốc mục "Yêu cầu Phần cứng & Phần mềm"](../../README.md#-1-yêu-cầu-phần-cứng--phần-mềm-prerequisites) — hướng dẫn build cụ thể cho cả 2 firmware.
