# Schema Dữ liệu ESP-NOW & Mô hình Cảm biến

> Tài liệu tra cứu **hình dạng dữ liệu** (shape/schema) trao đổi giữa 2 lớp của hệ thống trên nhánh hiện tại: `sensor-node` → `waveshare-screen` qua **ESP-NOW trực tiếp** (không qua cloud), cộng với struct nội bộ `sensor_model` mà dữ liệu ESP-NOW được ánh xạ vào trên `waveshare-screen`. Đây là tài liệu về **cấu trúc dữ liệu**, không phải về ngưỡng cảnh báo hay cách cấu hình phần mềm — xem [`docs/API_GUIDE.md`](../API_GUIDE.md) cho các chủ đề đó.
>
> **Đường CoreIoT/MQTT cũ (mục 1-2 phiên bản trước) hiện không hoạt động trên nhánh này** — xem [`docs/architecture/ESPNOW_NETWORK.md`](ESPNOW_NETWORK.md) để biết lý do (tránh xung đột Wi-Fi channel ESP-NOW) và [ADR 0003](../adr/0003-coreiot-cloud-platform.md) cho bối cảnh lịch sử. Code `CoreiotClient`/`coreiot_client`/Rule-Chain vẫn còn trong cây mã nguồn (không bị xoá) để khôi phục sau này, nhưng không được gọi từ nhánh hiện tại.

## Mục lục

1. [`sensor-node` → `waveshare-screen` (ESP-NOW telemetry)](#1-sensor-node--waveshare-screen-esp-now-telemetry)
2. [Hazard đánh giá cục bộ trên `waveshare-screen`](#2-hazard-đánh-giá-cục-bộ-trên-waveshare-screen)
3. [`sensor_model` — struct nội bộ trên `waveshare-screen`](#3-sensor_model--struct-nội-bộ-trên-waveshare-screen)
4. [Khoảng trống đã biết: chỉ 3/6 slot `sensor_model` có dữ liệu sống](#4-khoảng-trống-đã-biết-chỉ-36-slot-sensor_model-có-dữ-liệu-sống)
5. [Đồng bộ ngưỡng cảnh báo](#5-đồng-bộ-ngưỡng-cảnh-báo)

---

## 1. `sensor-node` → `waveshare-screen` (ESP-NOW telemetry)

**Cơ chế**: `esp_now_send()` trực tiếp từ `sensor-node` tới MAC của `waveshare-screen` — không qua Wi-Fi AP, không có broker, không có bước trung gian nào ở giữa. Định nghĩa nguồn dùng chung: [`docs/architecture/ESPNOW_NETWORK.md`](ESPNOW_NETWORK.md) (struct phải được khai báo trùng khớp thủ công ở cả 2 phía vì 2 project không share include path).
**Tần suất**: mỗi `ESPNOW_SEND_INTERVAL_MS` (500ms, 2Hz).
**Định dạng**: struct nhị phân packed (không phải JSON), luôn mang đủ 6 slot — slot nào không có dữ liệu hợp lệ thì `valid[i]=0` thay vì bị lược bỏ khỏi payload (JSON cũ bỏ hẳn key; struct cố định kích thước bắt buộc phải giữ chỗ).

```c
typedef struct __attribute__((packed)) {
    float   distance_cm[6];   // idx: 0=front,1=rear,2=left_front,3=left_rear,4=right_front,5=right_rear
    uint8_t valid[6];         // 1 = giá trị hợp lệ, 0 = cảm biến lỗi/chưa lắp phần cứng ("null")
} espnow_sensor_msg_t;
```

| Slot (idx) | Field | Ghi chú |
| --- | --- | --- |
| 0 | `front` | S1 — có phần cứng thật (`SENSOR_PINS[0]` trên sensor-node). |
| 1 | `rear` | S2 — chưa lắp phần cứng, luôn `valid=0`. |
| 2 | `left_front` | S3 — có phần cứng thật (`SENSOR_PINS[1]`). |
| 3 | `left_rear` | S4 — chưa lắp phần cứng, luôn `valid=0`. |
| 4 | `right_front` | S5 — có phần cứng thật (`SENSOR_PINS[2]`). |
| 5 | `right_rear` | S6 — chưa lắp phần cứng, luôn `valid=0`. |

**Khoảng trống hiện tại**: kiến trúc phần mềm hỗ trợ tối đa 6 cảm biến (`SENSOR_PINS[]`, `sensor_id_t` phía `waveshare-screen`), nhưng **chỉ 3/6 cảm biến được lắp phần cứng thật** (S1, S3, S5). `sensor-node` set `valid=1` cho 3 slot này qua mảng ánh xạ `SENSOR_ESPNOW_SLOT[SENSOR_COUNT]` (`EspNowConfig.h`); 3 slot còn lại luôn `valid=0`. Mở rộng thêm cảm biến: [`docs/API_GUIDE.md` mục 3.2](../API_GUIDE.md#32-thêmbớt-cảm-biến-trên-sensor-node).

## 2. Hazard đánh giá cục bộ trên `waveshare-screen`

Không còn Rule-Chain CoreIoT tính `vehicle_detected`/`warning_status`/`relay`/`buzzer` trên cloud rồi đẩy xuống qua MQTT shared attributes (như phiên bản trước ESP-NOW). Trên nhánh hiện tại, `waveshare-screen` tự đánh giá hazard **hoàn toàn cục bộ** ngay khi nhận được message ESP-NOW:

- `on_data_recv()` (`firmware/waveshare-screen/src/main.c`) parse `espnow_sensor_msg_t`, gọi `ui_dashboard_update_sensor()` cho slot `valid=1` hoặc `ui_dashboard_clear_sensor()` cho slot `valid=0`.
- `evaluate_hazard()` (`firmware/waveshare-screen/components/ui_dashboard/ui_dashboard.c`) quét cả 6 slot `sensor_model`, bỏ qua slot `is_stale` (chưa từng có dữ liệu hoặc vừa bị `clear`), lấy `zone` tệ nhất (`SENSOR_ZONE_SAFE/CAUTION/DANGER`, xem mục 3) để hiển thị banner "OVERALL: ...".
- Badge header hiển thị trạng thái liên kết ESP-NOW (`ui_dashboard_set_espnow_status()`) — chuyển "NO LINK" nếu quá 1.5s không nhận được message nào (esp_timer watchdog trong `main.c`), thay cho badge Wi-Fi/MQTT cũ.
- Buzzer vật lý trên `sensor-node` vẫn hoàn toàn độc lập, cục bộ, không phụ thuộc `waveshare-screen` hay đường ESP-NOW này — xem [`firmware/sensor-node/README.md`](../../firmware/sensor-node/README.md#buzzer-cảnh-báo-cục-bộ-không-qua-mạng).

## 3. `sensor_model` — struct nội bộ trên `waveshare-screen`

**Không phải payload ESP-NOW trực tiếp** — đây là struct trong bộ nhớ (RAM) trên `waveshare-screen`, nơi `espnow_sensor_msg_t` nhận được từ mục 1 được `on_data_recv()` (`firmware/waveshare-screen/src/main.c`) ánh xạ vào qua `ui_dashboard_update_sensor()`/`sensor_model_set_distance()` (slot `valid=1`) hoặc `ui_dashboard_clear_sensor()`/`sensor_model_clear()` (slot `valid=0`). Định nghĩa: [`components/sensor_model/include/sensor_model.h`](../../firmware/waveshare-screen/components/sensor_model/include/sensor_model.h). API đầy đủ: [`docs/API_GUIDE.md` mục 2.1](../API_GUIDE.md#21-sensor_model--mô-hình-dữ-liệu-6-cảm-biến-thread-safe).

### `sensor_id_t` — 6 slot cảm biến (khớp thứ tự S1..S6)

| Giá trị enum | Index | Vị trí lắp (compass) | Góc lắp `offset_deg` | Slot ESP-NOW tương ứng (mục 1) |
| --- | --- | --- | --- | --- |
| `SENSOR_ID_FRONT` | 0 | S1 — trước | 0° | `front` (0) — **có dữ liệu sống** |
| `SENSOR_ID_REAR` | 1 | S2 — sau | 180° | `rear` (1) — *(chưa lắp — xem mục 4)* |
| `SENSOR_ID_LEFT_FRONT` | 2 | S3 — trước-trái | -90° (trái) | `left_front` (2) — **có dữ liệu sống** |
| `SENSOR_ID_LEFT_REAR` | 3 | S4 — sau-trái | -90° (trái) | `left_rear` (3) — *(chưa lắp)* |
| `SENSOR_ID_RIGHT_FRONT` | 4 | S5 — trước-phải | +90° (phải) | `right_front` (4) — **có dữ liệu sống** |
| `SENSOR_ID_RIGHT_REAR` | 5 | S6 — sau-phải | +90° (phải) | `right_rear` (5) — *(chưa lắp)* |

Mỗi phần tử (`sensor_reading_t`) gồm: `distance_cm` (uint16_t), `offset_deg` (góc lắp cố định, dùng để vẽ vị trí trên canvas xe 2D), `is_stale` (cờ đánh dấu dữ liệu cũ/chưa cập nhật). Toàn bộ struct 6 phần tử bảo vệ bằng **1 mutex FreeRTOS dùng chung** cho mọi task đọc/ghi (network callback ghi, UI task đọc).

### Ngưỡng phân vùng (zone) — `sensor_model_classify()`

| Zone | Điều kiện | Ý nghĩa hiển thị |
| --- | --- | --- |
| `SENSOR_ZONE_SAFE` | `distance_cm > 100` | An toàn — không cảnh báo. |
| `SENSOR_ZONE_CAUTION` | `30 <= distance_cm <= 100` | Vùng chú ý — hiển thị màu cảnh báo nhẹ trên arc. |
| `SENSOR_ZONE_DANGER` | `distance_cm < 30` | Vùng nguy hiểm — nhấp nháy/màu đỏ trên arc. |

Ngưỡng này **hardcode trong mã nguồn C** (`firmware/waveshare-screen/components/sensor_model/sensor_model.c`), không phải tham số cấu hình — khác và **độc lập** với ngưỡng WARNING/DANGER 50cm/20cm của buzzer trên `sensor-node` (xem mục 5 bên dưới).

## 4. Khoảng trống đã biết: chỉ 3/6 slot `sensor_model` có dữ liệu sống

Kiến trúc phần mềm hỗ trợ đầy đủ 6 cảm biến ở **cả 3 lớp** (mảng `SENSOR_PINS[]` phía `sensor-node`, 6 field `sensor_id_t` phía `sensor_model`, 6 cung `lv_arc` trên UI) — nhưng dữ liệu thật hiện chỉ chảy qua **3 slot**: `SENSOR_ID_FRONT`, `SENSOR_ID_LEFT_FRONT` và `SENSOR_ID_RIGHT_FRONT`, vì:

1. Chỉ 3/6 cảm biến vật lý (S1, S3, S5) được lắp trên phần cứng thật.
2. `sensor-node` chỉ set `valid=1` cho 3 slot ESP-NOW tương ứng qua `SENSOR_ESPNOW_SLOT[]` (mục 1).
3. `waveshare-screen` gọi `ui_dashboard_clear_sensor()` cho các slot `valid=0` — không giữ lại giá trị cũ.

Kết quả: 3 slot còn lại (`SENSOR_ID_REAR`, `SENSOR_ID_LEFT_REAR`, `SENSOR_ID_RIGHT_REAR`) **luôn ở trạng thái "no data"** (`is_stale=true`, arc màu xám trung tính, sidebar hiện "-- cm") trên `waveshare-screen` — không phải lỗi, mà là hệ quả trực tiếp của việc chưa lắp đủ phần cứng, và `evaluate_hazard()` chủ động bỏ qua các slot này. **Người đóng góp mới không nên giả định cả 6 cảm biến đều "sống"** khi đọc code UI hay debug dashboard — 3 cung còn lại trên canvas xe sẽ luôn hiển thị trạng thái "no data" cho tới khi có phần cứng bổ sung và cả 3 lớp trên được mở rộng đồng bộ (hướng dẫn thêm cảm biến: [`docs/API_GUIDE.md` mục 3.2](../API_GUIDE.md#32-thêmbớt-cảm-biến-trên-sensor-node)).

## 5. Đồng bộ ngưỡng cảnh báo

File này mô tả **hình dạng** dữ liệu (field nào, kiểu gì) — không lặp lại giá trị ngưỡng cụ thể (20cm/50cm cho WARNING/DANGER buzzer, hay 30cm/100cm cho SAFE/CAUTION/DANGER ở mục 3). Trên nhánh hiện tại, ngưỡng buzzer WARNING/DANGER chỉ tồn tại **cục bộ trên `sensor-node`** (`Config.h`) — không còn bản sao trên Rule-Chain CoreIoT vì đường MQTT không hoạt động (mục 1-2), nên không còn nguy cơ trôi lệch 2 nơi như phiên bản trước. Ngưỡng zone SAFE/CAUTION/DANGER của `sensor_model` (mục 3) vẫn là 1 hằng số riêng, độc lập, hardcode trên `waveshare-screen`. Chi tiết cách sửa: [`docs/API_GUIDE.md` mục 3.4](../API_GUIDE.md#34-đổi-ngưỡng-cảnh-báo-warningdanger-buzzer--rule-chain--ui).
