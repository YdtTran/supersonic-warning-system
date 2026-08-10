# ESP-NOW Network Config

Thông tin dùng chung giữa `firmware/sensor-node` và `firmware/waveshare-screen`
khi giao tiếp qua ESP-NOW (đường truyền cục bộ trực tiếp giữa 2 board, **tách
biệt khỏi luồng MQTT/CoreIoT** — trong nhánh `feature/espnow-sensor-screen-link`,
sensor-node **tạm ngắt hẳn** kết nối WiFi STA/MQTT tới CoreIoT để tránh xung đột
WiFi channel với ESP-NOW). Hai project dùng build system riêng (ESP-IDF vs
PlatformIO/Arduino), không share include path, nên file này là nguồn thông tin
dùng chung duy nhất — không có header dùng chung, struct phải được định nghĩa
trùng khớp thủ công ở cả 2 phía.

## Devices & Roles

| Device            | Role         | MAC Address         | Ghi chú |
|-------------------|--------------|----------------------|---------|
| sensor-node       | **Sender**   | `80:b5:4e:e2:00:f4`  | Có thể đổi nếu flash lại/đổi board |
| waveshare-screen  | **Receiver** | `64:e8:33:7c:3f:e0`  | Cố định theo phần cứng board — là MAC đích (`ESPNOW_PEER_MAC`) mà sensor-node cần khai báo |

sensor-node chủ động gửi (`esp_now_send`) tới địa chỉ MAC của waveshare-screen mỗi
`ESPNOW_SEND_INTERVAL_MS` (500ms); waveshare-screen chỉ lắng nghe
(`esp_now_register_recv_cb`), không cần `esp_now_add_peer` vì không gửi ngược.

## Channel

Cả 2 bên phải cùng Wi-Fi channel: **1**.

## Message schema — `espnow_sensor_msg_t`

Payload nhị phân cố định độ dài (packed struct), mang đủ **6 vị trí cảm biến**
theo đúng mô hình `sensor_model`/`ui_dashboard` bên waveshare-screen
(`sensor_id_t` trong `firmware/waveshare-screen/components/sensor_model/include/sensor_model.h`),
— cả 6 slot đều đã có cảm biến vật lý trên sensor-node:

```c
typedef struct __attribute__((packed)) {
    float   distance_cm[6];   // idx: 0=front,1=rear,2=left_front,3=left_rear,4=right_front,5=right_rear
    uint8_t valid[6];         // 1 = giá trị hợp lệ, 0 = cảm biến lỗi/mất tín hiệu ("null")
} espnow_sensor_msg_t;
```

Ngữ nghĩa `valid[i]=0` tương đương việc payload CoreIoT (JSON) cũ bỏ hẳn key đó
(`{"left_front":85.3,"right_front":142.0}`) thay vì gửi `0`: cảm biến lỗi/mất
tín hiệu **hoặc chưa có phần cứng lắp**. sensor-node hiện có đủ phần cứng ở
cả 6 vị trí (`front`, `left_front`, `right_front`, `left_rear`, `right_rear`,
`rear`) — ánh xạ qua mảng `SENSOR_ESPNOW_SLOT[SENSOR_COUNT]` trong
`EspNowConfig.h` (đúng thứ tự vật lý `SENSOR_PINS[]` trong `Config.h`).
`valid[i]=0` vẫn có thể xảy ra cho một slot có phần cứng nếu cảm biến đó mất
tín hiệu tạm thời (xem `RESET_AFTER_INVALID` trong `Config.h`).
Mỗi khi thêm/bớt cảm biến vật lý, cập nhật `SENSOR_PINS[]` (`Config.h`) và
`SENSOR_ESPNOW_SLOT[]` (`EspNowConfig.h`) đồng thời theo **đúng thứ tự vật
lý** — hai mảng lệch nhau không gây lỗi build, chỉ gây sai lệch nhãn cảm
biến hiển thị trên dashboard (xem sự cố đã gặp trong
[`docs/logs/SENSOR_NODE_GPIO47_48_PSRAM_LOG.md`](../logs/SENSOR_NODE_GPIO47_48_PSRAM_LOG.md)).

Struct này được định nghĩa trùng khớp ở cả 2 phía:
- `firmware/sensor-node/include/EspNowConfig.h`
- `firmware/waveshare-screen/src/main.c`

## Đồng bộ MAC khi board đổi

- **waveshare-screen đổi board/reflash** (MAC đích thay đổi): mở Serial monitor
  waveshare-screen lúc boot để lấy MAC hiện tại (in ra qua `esp_wifi_get_mac`
  trong `firmware/waveshare-screen/src/main.c`), copy giá trị đó vào
  `ESPNOW_PEER_MAC` tại `firmware/sensor-node/include/EspNowConfig.h`, rồi cập
  nhật lại bảng MAC ở trên.
- **sensor-node đổi board/reflash**: dùng `readMacAddress()` trong
  `firmware/sensor-node/src/main.cpp` để lấy MAC mới, chỉ cần cập nhật bảng ở
  trên cho đúng thông tin tham khảo (waveshare-screen là receiver, không cần
  biết MAC của sensor-node để hoạt động).

## Source locations

- `ESPNOW_PEER_MAC` (địa chỉ đích sensor-node gửi tới): `firmware/sensor-node/include/EspNowConfig.h`
- `readMacAddress()` (in MAC của chính sensor-node): `firmware/sensor-node/src/main.cpp`
- `espnow_sensor_msg_t` (định nghĩa struct phía nhận): `firmware/waveshare-screen/src/main.c`

---

## Đặc tính kỹ thuật ESP-NOW

Tham khảo chung khi cần đánh giá xem ESP-NOW có đáp ứng được nhu cầu mở rộng
(thêm cảm biến, thêm board, tăng tần suất gửi...) hay không. Số liệu constant
lấy trực tiếp từ header `esp_now.h` (ESP-IDF, cả 2 project dùng chung phiên
bản chip ESP32-S3) — không phải ước lượng.

### Phần cứng & yêu cầu vận hành

- **Không cần Access Point / router**: ESP-NOW là giao thức lớp 2 (chạy trên
  cùng nền Wi-Fi radio, dùng vendor-specific action frame của 802.11) hoạt
  động độc lập với việc kết nối AP. Trên nhánh này cả 2 board chỉ set
  `WiFi.mode(WIFI_STA)` / `esp_wifi_set_mode(WIFI_MODE_STA)` để "mở" radio,
  **không gọi `WiFi.begin()`/kết nối AP nào** — xem `EspNowClient::begin()`
  (sensor-node) và `networkTask()` trong `src/main.c` (waveshare-screen).
- **Cùng dòng chip Espressif hỗ trợ Wi-Fi** (ESP32/ESP32-S2/S3/C3/C6...). Cả
  2 board trong project đều là ESP32-S3.
- **Bắt buộc cùng Wi-Fi channel** (xem mục Channel ở trên) — khác channel thì
  không nhận được gói tin dù đúng MAC đích, vì ESP-NOW không tự động dò kênh
  như quá trình kết nối AP thông thường.
- Sender cần `esp_now_add_peer()` khai báo MAC đích trước khi gửi; receiver
  thuần túy (không gửi ngược) chỉ cần `esp_now_register_recv_cb()`, không cần
  add peer — đúng như cách waveshare-screen (receiver-only) đang làm.

### Tầm hoạt động (range)

Không có con số cố định trong chuẩn — phụ thuộc công suất phát, anten, vật
cản và nhiễu môi trường, tương tự Wi-Fi thông thường vì dùng chung lớp vật
lý 802.11:

| Điều kiện | Tầm ước tính thực tế |
| :--- | :--- |
| Trong nhà, nhiều vật cản (tường, kim loại) | ~20–30m |
| Trong nhà, ít vật cản | ~50–70m |
| Ngoài trời, trực xạ (line-of-sight) | ~100–220m (Espressif công bố tới ~480m với anten định hướng ngoài trời trong điều kiện lý tưởng) |
| Trên xe/thiết bị di chuyển, nhiều nhiễu kim loại/động cơ | Giảm mạnh so với bảng trên — nên đo thực tế tại vị trí lắp |

Vì bố trí `sensor-node` và `waveshare-screen` trên cùng một xe (khoảng cách
thực tế chỉ vài mét), tầm hoạt động **không phải yếu tố giới hạn** của hệ
thống này; rủi ro thực tế lớn hơn là **nhiễu/che khuất bởi khung xe/động cơ
kim loại** — nếu gặp mất kết nối ESP-NOW chập chờn (`ui_dashboard_set_espnow_status(false)`
xuất hiện dù 2 board ở gần nhau), ưu tiên kiểm tra vị trí anten/che chắn kim
loại trước khi nghi ngờ do khoảng cách.

### Kích thước payload / buffer

| Constant (`esp_now.h`) | Giá trị | Ghi chú |
| :--- | :--- | :--- |
| `ESP_NOW_MAX_DATA_LEN` | **250 bytes** | Giới hạn payload mỗi lần gửi, ESP-NOW v1.0 (mặc định, không cần cấu hình gì thêm) |
| `ESP_NOW_MAX_DATA_LEN_V2` | **1470 bytes** | ESP-NOW v2.0 — cần cả 2 bên hỗ trợ + bật cấu hình tương ứng, **dự án này chưa dùng** |
| `ESP_NOW_ETH_ALEN` | 6 bytes | Độ dài MAC address |
| `ESP_NOW_KEY_LEN` | 16 bytes | Độ dài khóa mã hoá LMK (nếu bật `encrypt`) |
| `ESP_NOW_MAX_TOTAL_PEER_NUM` | **20** | Số peer tối đa 1 thiết bị có thể `esp_now_add_peer()` |
| `ESP_NOW_MAX_ENCRYPT_PEER_NUM` | **6** | Số peer tối đa được mã hoá (CCMP) trong tổng số 20 |

**Payload thực tế của dự án**: `espnow_sensor_msg_t` gồm `float[6]` (24 bytes)
và `uint8_t[6]` (6 bytes), tổng cộng **30 bytes/gói**, packed — chỉ chiếm
**12% giới hạn 250 bytes** của ESP-NOW v1.0. Còn nhiều dư địa nếu cần mở rộng
thêm field (ví dụ thêm nhiệt độ, trạng thái pin...) mà không cần bật v2.0 hay
chia gói.

`esp_now_send()` không có cơ chế buffer/queue nội bộ cho việc gửi liên tục —
mỗi lần gọi là một lần gửi rời rạc; ứng dụng tự quản lý nhịp gửi bằng
`ESPNOW_SEND_INTERVAL_MS` (500ms, xem `networkTask()` trong `main.cpp` của
sensor-node).

### Độ tin cậy & độ trễ

- **Unicast** (gửi đích danh MAC, như dự án này đang dùng) có xác nhận ở lớp
  MAC (802.11 ACK) và callback `esp_now_register_send_cb()` báo
  `ESP_NOW_SEND_SUCCESS`/`ESP_NOW_SEND_FAIL` — nhưng đây chỉ là ACK "đã tới
  được lớp MAC của máy nhận", **không đảm bảo tầng ứng dụng đã xử lý xong**
  (không có retry/ACK ứng dụng tự động). Broadcast (MAC `FF:FF:FF:FF:FF:FF`)
  thì hoàn toàn không có ACK.
- Độ trễ thực tế mỗi gói thường **dưới 10ms** ở khoảng cách gần (không qua
  AP/router nên không có overhead DHCP/routing) — nhanh hơn đáng kể so với
  MQTT qua Wi-Fi STA + broker, phù hợp cho cảnh báo va chạm cần phản hồi
  nhanh.
- Không đảm bảo thứ tự gói tin nếu gửi dồn dập liên tiếp mà không chờ
  callback — dự án này gửi cách nhau 500ms nên không gặp vấn đề này.
- watchdog phía waveshare-screen (`ESPNOW_LINK_TIMEOUT_MS` = 1500ms trong
  `src/main.c`) tự phát hiện mất kết nối và chuyển badge sang "NO LINK" nếu
  không nhận được gói nào trong 1.5–2s — bù cho việc ESP-NOW không có
  heartbeat/keep-alive sẵn có ở tầng giao thức.

### Bảo mật

ESP-NOW hỗ trợ mã hoá CCMP (AES-128) theo từng peer qua `peerInfo.encrypt` +
`peerInfo.lmk`. Dự án hiện set `encrypt = false` (xem
`EspNowClient::begin()`) — dữ liệu khoảng cách gửi ở dạng plaintext trên
sóng. Chấp nhận được cho phạm vi cục bộ trên cùng 1 xe, nhưng **không dùng
cấu hình này nếu mở rộng ra khoảng cách xa hơn hoặc dữ liệu nhạy cảm hơn**.
