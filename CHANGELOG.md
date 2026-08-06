# Changelog

Tất cả thay đổi đáng chú ý của dự án `supersonic-sensor-ACLAB` được ghi lại ở đây, theo định dạng gợi ý từ [Keep a Changelog](https://keepachangelog.com/vi/1.1.0/).

> **Quan hệ với các tài liệu lịch sử khác:**
>
> - File này là bản tóm tắt **ngắn gọn, theo mốc thời gian, hướng người dùng** — mỗi mục chỉ 1-2 dòng, trả lời "cái gì đã thay đổi và khi nào".
> - [`docs/logs/`](docs/logs/) là nhật ký triển khai **chi tiết** (dev diary), mỗi file ghi lại đầy đủ quá trình debug/quyết định của **một** nhiệm vụ/sự cố cụ thể — đọc khi cần hiểu **tại sao** và **làm thế nào**, không chỉ **cái gì**.
> - [`report/README.md` mục 9](report/README.md#9-nhật-ký--lịch-sử-phát-triển) là bản tường thuật văn xuôi cho báo cáo kỹ thuật, kể lại câu chuyện phát triển theo mạch logic (không nhất thiết theo thứ tự thời gian tuyệt đối).
>
> **Không dùng SemVer**: dự án hiện chưa có tag/release chính thức, nên các mục dưới đây được nhóm theo **ngày** (`## [YYYY-MM-DD] - <mô tả ngắn>`), mới nhất ở trên, thay vì theo số phiên bản. Ngày được đối chiếu từ `git log` thực tế của repo, không phải ước lượng.

## [Unreleased]

### Changed

- `sensor-node` và `waveshare-screen` chuyển sang giao tiếp trực tiếp qua **ESP-NOW** (channel cố định, không qua Wi-Fi AP/MQTT/CoreIoT) thay cho publish/subscribe MQTT qua CoreIoT Rule-Chain — xem [`docs/architecture/ESPNOW_NETWORK.md`](docs/architecture/ESPNOW_NETWORK.md). `CoreiotClient`/`coreiot_client`/Rule-Chain vẫn còn trong cây mã nguồn, không bị xoá, để khôi phục sau này.
- `sensor-node` lắp thêm cảm biến thứ 3 (S1, Front) — `SENSOR_COUNT` 2→3, ánh xạ ESP-NOW qua `SENSOR_ESPNOW_SLOT[]` mới trong `EspNowConfig.h`.
- `waveshare-screen` tự đánh giá hazard cục bộ ("OVERALL" banner qua `evaluate_hazard()`), không còn phụ thuộc `vehicle_detected`/`warning_status`/`relay` do Rule-Chain CoreIoT tính.

### Added

- `sensor_model_clear()` / `ui_dashboard_clear_sensor()`: đánh dấu 1 cảm biến "no data" khi ESP-NOW báo `valid=0` cho slot đó (arc màu xám trung tính, sidebar "-- cm"), thay vì giữ lại khoảng cách cũ.
- `ui_dashboard_set_espnow_status()`: badge header "ESP-NOW: LINKED"/"NO LINK", cập nhật bởi watchdog `esp_timer` 1s (ngưỡng mất liên kết 1.5s).

## [2026-08-04] - Refactor `waveshare-screen` sang PlatformIO/ESP-IDF thuần, buzzer, báo cáo kỹ thuật

### Added

- Buzzer vật lý cục bộ trên `sensor-node` (GPIO48): kêu 3s/lần khi WARNING, 1s/lần khi DANGER, không qua round-trip cloud.
- Rule-Chain CoreIoT: thêm field `buzzer` (mirror `relay`) để `waveshare-screen` hiển thị đồng bộ trạng thái còi.
- `ui_dashboard`: hiện SSID Wi-Fi đang kết nối; tab SYSTEM đầy đủ (device/network/cloud key đã che, firmware/IDF version, flash/heap/uptime).
- `prototypes/pulse-read-prototype`: đọc trực tiếp xung Trig/Echo qua GPIO (JSN-SR04T Mode 3), thay vì giải mã khung UART.
- `report/README.md` + `report/report.tex`/`report.pdf`: báo cáo kỹ thuật đầy đủ (kiến trúc, phần cứng, nhật ký phát triển, hạn chế).

### Changed

- `waveshare-screen` chuyển sang layout PlatformIO (`main/` → `src/`, `platformio.ini`/`boards/yolo_uno.json` mới) — vẫn `framework = espidf` thuần, không Arduino (xem [ADR 0002](docs/adr/0002-espidf-pure-for-waveshare-screen.md)).
- `sensor-node`: tăng `MAX_DISTANCE_CM` 450→500cm, `ECHO_TIMEOUT_US` 40ms, giảm `Serial.printf` trong vòng đo tốc độ cao để giảm trễ I/O.

### Fixed

- `esp_lv_adapter_lock(-1)` (chờ vô hạn) trong callback Wi-Fi/MQTT đổi thành timeout 100ms — tránh nguy cơ deadlock toàn hệ thống nếu task LVGL bị kẹt.

## [2026-08-03] - Tái cấu trúc thư mục repo, dashboard 6-cảm-biến, publish 2 cảm biến lên CoreIoT

### Changed

- Tái cấu trúc thư mục gốc: `sensor-node`/`waveshare-screen` → `firmware/`, `coreiot/` → `cloud/coreiot/`, `sub/` → `reference/`, gộp version log rải rác vào `docs/logs/` (dùng `git mv` giữ lịch sử).
- `waveshare-screen` viết lại thành 3 component ESP-IDF độc lập: `sensor_model`, `coreiot_client`, `ui_dashboard`; dashboard va chạm 6-cảm-biến kiểu "no-zone" quanh xe tải, tab COLLISION/SYSTEM.
- `prototypes/water-level-uart`: thay bộ lọc Kalman bằng thuật toán Cluster+EMA của `sensor-node` (xem [ADR 0004](docs/adr/0004-cluster-ema-distance-filter.md)).

### Added

- Publish MQTT 2 cảm biến S3 (`left_front`)/S5 (`right_front`) từ `sensor-node` lên CoreIoT, cập nhật Rule-Chain tương ứng để nhận cả 2 key.

### Fixed

- Regression `SENSOR_COUNT=1` khiến mất dữ liệu cảm biến S5 (`right_front`).
- Đổi SSID Wi-Fi `ACLAB` → `HCMUT-MEETING` trên cả 2 firmware, khắc phục lỗi mất kết nối `NO_AP_FOUND (201)` quan sát được khi debug trực tiếp.

## [2026-07-30] - Prototype đo UART & công cụ debug (giai đoạn ~30/07)

### Added

- `prototypes/water-level-uart`: prototype đo mực nước JSN-SR04T-V3 qua UART, bộ lọc Median-5 (sau đó được thay bằng Cluster+EMA — xem mục 2026-08-03 ở trên).
- `tools/plot_ultrasonic_distance.py`: công cụ vẽ đồ thị real-time giá trị khoảng cách đọc qua UART.

### Changed

- Chuyển cảm biến JSN-SR04T từ Software UART sang **Hardware UART1 (GPIO 44)** để tăng độ ổn định đọc dữ liệu.

## [2026-07-29] - Tích hợp Rule-Chain CoreIoT → màn hình, audit kiến trúc LVGL (giai đoạn ~29/07)

### Added

- Rule-Chain CoreIoT: định tuyến dữ liệu đã xử lý xuống `waveshare-screen`, hiển thị cảnh báo trên dashboard, cấu hình root-of-trust cho server.
- `docs/architecture/`: audit ví dụ chính hãng Waveshare (Session 1), audit kiến trúc `lv_demos` của LVGL (Session 2), SOP pipeline phát triển UI cảnh báo real-time (Session 3).

## [2026-07-28] - Khởi tạo tài liệu & quản lý key bí mật (giai đoạn ~28/07)

### Added

- README gốc + README riêng cho từng sub-project, template `AGENTS.md` cho quy trình cross-device setup.
- `config/keys.json` (gitignored) + `config/keys.template.json` quản lý Device Access Token CoreIoT tách khỏi Git.
- `tools/test_mqtt_coreiot.py`: script kiểm thử publish dữ liệu MQTT giả lập lên CoreIoT.
- `reference/lcd-example` làm git submodule (ví dụ chính hãng Waveshare ESP32-S3 Touch LCD 7").

[unreleased]: https://github.com/YdtTran/supersonic-warning-system/compare/main...HEAD
