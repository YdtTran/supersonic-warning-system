# 0003. CoreIoT (ThingsBoard) làm nền tảng cloud IoT

**Trạng thái**: Accepted
**Ngày**: giai đoạn phát triển ban đầu (~2026-07-28, thiết lập kết nối MQTT + token đầu tiên tới CoreIoT)

## Bối cảnh (Context)

Hệ thống cần một tầng trung gian giữa `sensor-node` (đo khoảng cách) và `waveshare-screen` (hiển thị cảnh báo) để: nhận telemetry qua MQTT, áp dụng logic ngưỡng cảnh báo (WARNING/DANGER dựa trên khoảng cách), và định tuyến kết quả đã xử lý xuống thiết bị hiển thị — mà không phải tự viết và vận hành một backend riêng (server xử lý message, lưu trữ timeseries, cơ chế publish/subscribe). Đây là dự án quy mô học thuật/prototype (ACLAB), ưu tiên tốc độ triển khai và chi phí vận hành bằng 0 hơn là khả năng mở rộng hay tuỳ biến sâu.

## Quyết định (Decision)

Dùng **CoreIoT** (`app.coreiot.io`) — một nền tảng IoT dựa trên **ThingsBoard** mã nguồn mở — làm cloud trung gian. Cụ thể:

- MQTT-native: cả 2 firmware kết nối trực tiếp bằng MQTT chuẩn (`PubSubClient` trên `sensor-node`, `esp-mqtt` trên `waveshare-screen`), dùng Device Access Token làm username, không cần backend tuỳ biến.
- **Rule-Chain** kéo-thả trên UI CoreIoT xử lý logic ngưỡng cảnh báo bằng JavaScript ngay trên giao diện (node `TbTransformMsgNode`), không cần viết/deploy service riêng.
- **MQTT Shared Attributes** (`notifyDevice=true`) dùng làm cơ chế đẩy dữ liệu đã xử lý xuống `waveshare-screen`, kết hợp node `TbChangeOriginatorNode` để đổi chủ thể bản tin giữa 2 thiết bị trong cùng 1 rule-chain.

## Hệ quả (Consequences)

**Tích cực:**
- Không cần triển khai/vận hành backend riêng — toàn bộ logic xử lý (ngưỡng, định tuyến) nằm trong Rule-Chain, sửa trực tiếp trên UI hoặc import/export JSON.
- MQTT-native nghĩa là cả 2 firmware dùng thư viện MQTT chuẩn, quen thuộc, không cần SDK riêng của nền tảng.
- Miễn phí cho quy mô prototype/học thuật hiện tại, phù hợp ngân sách dự án.

**Đánh đổi:**
- Logic nghiệp vụ quan trọng (công thức tính `vehicle_detected`/`warning_status`) nằm trong JS script chỉnh sửa qua UI CoreIoT, **tách rời khỏi version control** của repo — file [`cloud/coreiot/rule_chain/supersonic_rule_chain.json`](../../cloud/coreiot/rule_chain/supersonic_rule_chain.json) chỉ là snapshot export thủ công, dễ lệch với bản đang chạy thật trên server nếu quên export lại sau khi sửa trên UI.
- Ngưỡng cảnh báo (20cm/50cm) phải đồng bộ thủ công giữa JS script trên CoreIoT và `Config.h` trên firmware — 2 nơi chỉnh sửa độc lập, dễ trôi lệch (xem [`docs/API_GUIDE.md` mục 3.4](../API_GUIDE.md#34-đổi-ngưỡng-cảnh-báo-warningdanger-buzzer--rule-chain--ui)).
- Node `Change Originator` định tuyến theo **tên thiết bị** (`entityNamePattern: "waveshare-screen"`), không phải ID cố định — nếu đổi tên thiết bị trên CoreIoT hoặc deploy sang tenant khác mà không đặt đúng tên, bản tin bị drop âm thầm không log lỗi rõ ràng.
- Kết nối MQTT hiện dùng port 1883 (không TLS) — chấp nhận được cho demo/lab nội bộ nhưng là nợ kỹ thuật bảo mật đã biết (xem [`SECURITY.md`](../../SECURITY.md)).
- Phụ thuộc vào một nhà cung cấp cloud bên thứ ba (`app.coreiot.io`) — nếu dịch vụ ngừng hoạt động hoặc đổi chính sách, cần tìm nền tảng ThingsBoard thay thế (tự host hoặc tenant khác).

## Tham khảo

- [`report/README.md` mục 4](../../report/README.md#4-công-cụ--framework) và [mục 7](../../report/README.md#7-cloud-coreiot-rule-chain) — lý do chọn CoreIoT và chi tiết Rule-Chain.
- [`docs/API_GUIDE.md` mục 4](../API_GUIDE.md#4-rule-chain-coreiot--cấu-hình--api-node) — danh sách node, script JS đầy đủ, lưu ý deploy sang tenant khác.
- [`docs/architecture/DATA_SCHEMA.md`](../architecture/DATA_SCHEMA.md) — schema đầy đủ của các payload MQTT trao đổi qua CoreIoT.
