# Chính sách Bảo mật (Security Policy)

> Dự án học thuật/prototype (ACLAB) — không có SLA bảo mật chính thức, nhưng các vấn đề dưới đây cần được người dùng lại repo này biết trước khi triển khai ngoài môi trường thử nghiệm.

## Phạm vi

Áp dụng cho toàn bộ firmware (`firmware/sensor-node`, `firmware/waveshare-screen`), cấu hình Rule-Chain (`cloud/coreiot/`) và script hỗ trợ (`tools/`) trong repo này.

## Vấn đề bảo mật đã biết (Known Issues)

### 1. Wi-Fi password & MQTT Access Token hardcode dạng plaintext, đã commit vào Git

- **Vị trí**: `firmware/sensor-node/include/CoreiotConfig.h`, `firmware/waveshare-screen/components/coreiot_client/include/coreiot_client.h`.
- **Rủi ro**: Bất kỳ ai có quyền đọc repo (kể cả sau khi xoá khỏi commit mới nhất — lịch sử Git vẫn còn) đều lấy được SSID/password Wi-Fi và Device Access Token CoreIoT, có thể publish dữ liệu giả hoặc chiếm quyền thiết bị trên tenant CoreIoT.
- **Đánh giá hiện tại**: Chấp nhận được ở quy mô prototype/học thuật nội bộ (`app.coreiot.io`, tenant thử nghiệm, không dữ liệu nhạy cảm thật).
- **Không chấp nhận được nếu**: đưa lên môi trường sản xuất, mở rộng ra thiết bị thật ngoài đường, hoặc repo chuyển sang public với tenant CoreIoT đang hoạt động.
- **Kế hoạch khắc phục**: chuyển các giá trị này sang NVS (runtime-provisioned) hoặc đọc từ `config/keys.json` (đã `.gitignore`) tại thời điểm build/runtime thay vì hardcode trong header đã commit. Xem chi tiết tại [`docs/API_GUIDE.md` mục 5](docs/API_GUIDE.md#5-lưu-ý-bảo-mật).

### 2. Kết nối MQTT không dùng TLS

- Broker `app.coreiot.io` hiện kết nối qua **port 1883 (MQTT thuần, không mã hoá)**, không phải `8883` (MQTTS/TLS).
- **Rủi ro**: Token và payload telemetry truyền dạng plaintext trên mạng, có thể bị nghe lén (đặc biệt nếu chạy qua Wi-Fi công cộng/không tin cậy).
- **Đánh giá hiện tại**: Chấp nhận được cho demo/lab nội bộ. CoreIoT hỗ trợ MQTTS trên port 8883 — nên bật khi triển khai thật.

### 3. `config/keys.json` chứa token cá nhân

- File này đã được `.gitignore`, nhưng nếu vô tình `git add -f` hoặc copy nhầm ra ngoài, token sẽ bị lộ. Không có cơ chế rotate token tự động — nếu nghi ngờ lộ token, thu hồi/tạo lại **Device Access Token** trực tiếp trên CoreIoT (Devices → chọn thiết bị → Manage credentials).

## Báo cáo lỗi bảo mật

Đây là repo học thuật quy mô nhỏ, không có kênh báo bảo mật riêng. Nếu phát hiện vấn đề bảo mật:

- Mở một GitHub Issue trên repo, gắn nhãn `security` (chỉ dùng cho vấn đề **không nhạy cảm**, ví dụ như các mục đã biết ở trên).
- Nếu vấn đề liên quan đến token/credential đang hoạt động (nhạy cảm), **không** đăng công khai — liên hệ trực tiếp maintainer qua GitHub trước.

## Ngoài phạm vi (Out of Scope)

- Bảo mật vật lý của thiết bị (ai có quyền truy cập USB/Serial đều có thể đọc/ghi firmware).
- Bảo mật hạ tầng CoreIoT/ThingsBoard (thuộc trách nhiệm nhà cung cấp dịch vụ).
