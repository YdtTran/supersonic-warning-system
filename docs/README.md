# Mục lục Tài liệu (`docs/`)

> Điểm vào cho toàn bộ tài liệu kỹ thuật của dự án. Nếu chỉ cần một điểm bắt đầu duy nhất: đọc [README.md gốc](../README.md) (tổng quan + quick start) → [`report/README.md`](../report/README.md) (kiến trúc & lý do thiết kế) → [`API_GUIDE.md`](API_GUIDE.md) (tra cứu API khi code).

## 1. Tài liệu chính (đọc trước)

| Tài liệu | Nội dung | Khi nào đọc |
| --- | --- | --- |
| [`README.md`](../README.md) (gốc) | Tổng quan hệ thống, kiến trúc repo, cài đặt & build/flash nhanh | Lần đầu tiếp cận dự án |
| [`report/README.md`](../report/README.md) | Báo cáo kỹ thuật đầy đủ: kiến trúc, phần cứng, quyết định thiết kế, nhật ký phát triển, hạn chế hiện tại | Cần hiểu **vì sao** hệ thống được thiết kế như hiện tại |
| [`report/report.pdf`](../report/report.pdf) / [`report.tex`](../report/report.tex) | Bản báo cáo trang trọng, chỉ mô tả trạng thái hiện tại (không log/lịch sử) | Nộp báo cáo / trình bày chính thức |
| [`API_GUIDE.md`](API_GUIDE.md) | API từng thư viện/component tự viết (chữ ký hàm, ví dụ code), cách cấu hình ESP-NOW/ngưỡng cảnh báo bằng phần mềm (mục Wi-Fi/MQTT/Rule-Chain đánh dấu "không dùng trên nhánh hiện tại") | Đang code, cần tra cứu API hoặc đổi cấu hình |
| [`../CONTRIBUTING.md`](../CONTRIBUTING.md) | Quy trình Git, coding convention, quy tắc ghi log khi hoàn thành việc | Trước khi commit/push thay đổi |
| [`../SECURITY.md`](../SECURITY.md) | Vấn đề bảo mật đã biết (token hardcode, MQTT không TLS), cách báo lỗi bảo mật | Trước khi triển khai ngoài môi trường lab, hoặc phát hiện lỗ hổng |
| [`../CHANGELOG.md`](../CHANGELOG.md) | Tóm tắt thay đổi theo mốc thời gian (Keep a Changelog, chưa có SemVer) | Muốn biết nhanh "gần đây đã đổi gì" mà không đọc hết `docs/logs/` |

## 2. `docs/architecture/` — Tài liệu kiến trúc & review

Phần lớn là tài liệu **audit/nghiên cứu trước khi triển khai** module `waveshare-screen` (tiếng Anh, viết trong giai đoạn khảo sát LVGL/ESP-IDF ban đầu) — tham khảo khi cần hiểu bối cảnh quyết định UI, không phải tài liệu kiến trúc "canonical" cập nhật liên tục (xem [`report/README.md` mục 2](../report/README.md#2-kiến-trúc-tổng-quan) cho kiến trúc hiện hành). Ngoại lệ là `DATA_SCHEMA.md` và `ESPNOW_NETWORK.md` — tài liệu schema chính thức, cập nhật theo hình dạng dữ liệu thật của hệ thống (nhánh hiện tại: ESP-NOW, không phải MQTT).

| File | Nội dung |
| --- | --- |
| [`ESPNOW_NETWORK.md`](architecture/ESPNOW_NETWORK.md) | Nguồn thông tin dùng chung ESP-NOW (đang dùng): MAC/channel 2 board, struct `espnow_sensor_msg_t`, cách đồng bộ khi đổi board |
| [`DATA_SCHEMA.md`](architecture/DATA_SCHEMA.md) | Schema chính thức payload ESP-NOW (`sensor-node` → `waveshare-screen`) và struct nội bộ `sensor_model`, kèm khoảng trống đã biết (chỉ 3/6 slot cảm biến có dữ liệu sống); có ghi chú đường MQTT/CoreIoT cũ không còn hoạt động |
| [`waveshare_examples_review.md`](architecture/waveshare_examples_review.md) | Kiểm kê & review các ví dụ mẫu ESP-IDF chính hãng Waveshare (`reference/lcd-example`) — I2C test, driver LCD/touch tham khảo |
| [`lvgl_demos_architecture_review.md`](architecture/lvgl_demos_architecture_review.md) | Audit kiến trúc `lv_demos` của LVGL, đối chiếu với nhu cầu UI cảnh báo va chạm |
| [`ui_development_pipeline.md`](architecture/ui_development_pipeline.md) | SOP xây dựng UI cảnh báo real-time bằng LVGL v9 (design tokens, quy trình phát triển) |
| [`PROGRESS_CHECKPOINT.md`](architecture/PROGRESS_CHECKPOINT.md) | Checkpoint tiến độ các phiên audit/thiết kế nói trên (session 1–4) |

## 2b. `docs/adr/` — Architecture Decision Records

Các quyết định kiến trúc quan trọng, trích xuất từ `report/README.md` mục 4 và mục 9 thành định dạng ngắn gọn có cấu trúc (Bối cảnh/Quyết định/Hệ quả) — xem [`docs/adr/README.md`](adr/README.md) để biết định dạng và quy ước đánh số. 4 ADR hiện có: [0001 PlatformIO thống nhất](adr/0001-platformio-unified-build-system.md), [0002 ESP-IDF thuần cho waveshare-screen](adr/0002-espidf-pure-for-waveshare-screen.md), [0003 CoreIoT làm nền tảng cloud](adr/0003-coreiot-cloud-platform.md), [0004 Bộ lọc Cluster+EMA](adr/0004-cluster-ema-distance-filter.md).

## 3. `docs/logs/` — Nhật ký triển khai (Implementation Logs)

Mỗi file ghi lại **một nhiệm vụ/tính năng/sự cố cụ thể**: mục tiêu, file đã sửa, kết quả build/flash/test, hướng dẫn vận hành. Đây là nhật ký chi tiết (dev diary), khác với changelog tóm tắt theo version — quy tắc tạo log mới xem [`CONTRIBUTING.md`](../CONTRIBUTING.md#ghi-log-triển-khai-implementation-logging).

Quy ước đặt tên: `docs/logs/<COMPONENT_hoặc_CHỦ_ĐỀ>_<MÔ_TẢ>_LOG.md`.

| File | Chủ đề |
| --- | --- |
| [`SENSOR_NODE_VERSION_LOG.md`](logs/SENSOR_NODE_VERSION_LOG.md) | Lịch sử phiên bản `firmware/sensor-node` |
| [`WAVESHARE_SCREEN_VERSION_LOG.md`](logs/WAVESHARE_SCREEN_VERSION_LOG.md) | Lịch sử phiên bản & troubleshooting `firmware/waveshare-screen` |
| [`waveshare-screen_ARDUINO_REFACTOR_LOG.md`](logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md) | Quyết định chuyển `waveshare-screen` sang PlatformIO + ESP-IDF thuần (không Arduino) |
| [`WAVESHARE_SCREEN_SESSION_LOG.md`](logs/WAVESHARE_SCREEN_SESSION_LOG.md) | Tổng kết phiên làm việc dashboard CoreIoT Monitor v1.0 |
| [`WAVESHARE_SCREEN_DEBUG_REPORT.md`](logs/WAVESHARE_SCREEN_DEBUG_REPORT.md) | Debug & khắc phục lỗi hệ thống màn hình Waveshare |
| [`COREIOT_LAPTOP_DEMO_LOG.md`](logs/COREIOT_LAPTOP_DEMO_LOG.md) | Demo giả lập `sensor-node` gửi MQTT từ laptop + viết Rule-Chain CoreIoT |
| [`COREIOT_SCREEN_ROUTING_LOG.md`](logs/COREIOT_SCREEN_ROUTING_LOG.md) | Định tuyến dữ liệu Rule-Chain → dashboard màn hình, badge trạng thái cảnh báo |
| [`DASHBOARD_LIBRARY_LOG.md`](logs/DASHBOARD_LIBRARY_LOG.md) | Tích hợp thư viện dashboard & layout gốc cho `waveshare-screen` |
| [`HARDWARE_UART_GPIO43_LOG.md`](logs/HARDWARE_UART_GPIO43_LOG.md) | Chuyển từ Software UART sang Hardware UART1 (GPIO 44) cho JSN-SR04T |
| [`SR04M2_UART_DRIVER_LOG.md`](logs/SR04M2_UART_DRIVER_LOG.md) | Chẩn đoán & sửa lỗi driver UART cảm biến SR04M-2 |
| [`WATER_LEVEL_MEDIAN5_LOG.md`](logs/WATER_LEVEL_MEDIAN5_LOG.md) | Prototype đo mực nước, bộ lọc Median-5 |

## 4. Tài liệu theo module (không nằm trong `docs/`)

| Module | Tài liệu |
| --- | --- |
| `firmware/sensor-node` | [`firmware/sensor-node/README.md`](../firmware/sensor-node/README.md) |
| `firmware/waveshare-screen` | [`firmware/waveshare-screen/README.md`](../firmware/waveshare-screen/README.md) |
| `cloud/coreiot/rule_chain/` (không dùng trên nhánh hiện tại) | [`cloud/coreiot/rule_chain/supersonic_rule_chain.json`](../cloud/coreiot/rule_chain/supersonic_rule_chain.json) + chi tiết tại [`API_GUIDE.md` mục 4](API_GUIDE.md#4-rule-chain-coreiot--cấu-hình--api-node) |

## 5. Khoảng trống tài liệu đã biết (chưa có, nên bổ sung)

- Sơ đồ đấu dây phần cứng thực tế (hiện chỉ có pinout board trần, xem [`report/README.md` mục 11](../report/README.md#11-ảnh-minh-hoạ--đề-xuất-bổ-sung)).

Đã bổ sung (trước đây từng nằm ở mục này):

- Architecture Decision Records (ADR) — xem [mục 2b](#2b-docsadr--architecture-decision-records) và [`docs/adr/`](adr/).
- Schema chính thức cho payload dùng chung giữa `sensor-node` ↔ `waveshare-screen` — hiện qua ESP-NOW, xem [`docs/architecture/ESPNOW_NETWORK.md`](architecture/ESPNOW_NETWORK.md) và [`docs/architecture/DATA_SCHEMA.md`](architecture/DATA_SCHEMA.md) (đường MQTT/Rule-Chain trước đó ghi chú là không hoạt động).
- `CHANGELOG.md` theo mốc thời gian (khác với nhật ký dev chi tiết ở mục 3) — xem [`../CHANGELOG.md`](../CHANGELOG.md).
