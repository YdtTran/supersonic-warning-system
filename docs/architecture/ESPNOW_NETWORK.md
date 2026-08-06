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
không chỉ 2 cảm biến vật lý hiện có trên sensor-node:

```c
typedef struct __attribute__((packed)) {
    float   distance_cm[6];   // idx: 0=front,1=rear,2=left_front,3=left_rear,4=right_front,5=right_rear
    uint8_t valid[6];         // 1 = giá trị hợp lệ, 0 = cảm biến lỗi/chưa lắp phần cứng
} espnow_sensor_msg_t;
```

Ngữ nghĩa `valid[i]=0` tương đương việc payload CoreIoT (JSON) cũ bỏ hẳn key đó
(`{"left_front":85.3,"right_front":142.0}`) thay vì gửi `0`: cảm biến lỗi/mất
tín hiệu **hoặc chưa có phần cứng lắp**. Hiện sensor-node chỉ có phần cứng ở
`left_front` (idx 2, S3) và `right_front` (idx 4, S5) — 4 slot còn lại
(`front, rear, left_rear, right_rear`) luôn gửi `valid=0`, giữ đúng khung 6
cảm biến để sẵn sàng mở rộng phần cứng sau này mà không phải đổi lại schema.

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
