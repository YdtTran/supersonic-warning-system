# Schema Dữ liệu MQTT & Mô hình Cảm biến

> Tài liệu tra cứu **hình dạng dữ liệu** (shape/schema) trao đổi giữa 3 lớp của hệ thống: `sensor-node` → CoreIoT → `waveshare-screen`, cộng với struct nội bộ `sensor_model` mà dữ liệu MQTT được ánh xạ vào trên `waveshare-screen`. Đây là tài liệu về **cấu trúc dữ liệu**, không phải về ngưỡng cảnh báo hay cách cấu hình phần mềm — xem [`docs/API_GUIDE.md`](../API_GUIDE.md) cho các chủ đề đó. Kiến trúc tổng thể & lý do thiết kế: [`report/README.md` mục 2](../../report/README.md#2-kiến-trúc-tổng-quan) và [mục 7](../../report/README.md#7-cloud-coreiot-rule-chain).

## Mục lục

1. [`sensor-node` → CoreIoT (MQTT telemetry)](#1-sensor-node--coreiot-mqtt-telemetry)
2. [CoreIoT Rule-Chain → `waveshare-screen` (MQTT shared attributes)](#2-coreiot-rule-chain--waveshare-screen-mqtt-shared-attributes)
3. [`sensor_model` — struct nội bộ trên `waveshare-screen`](#3-sensor_model--struct-nội-bộ-trên-waveshare-screen)
4. [Khoảng trống đã biết: chỉ 2/6 slot `sensor_model` có dữ liệu sống](#4-khoảng-trống-đã-biết-chỉ-26-slot-sensor_model-có-dữ-liệu-sống)
5. [Đồng bộ ngưỡng cảnh báo](#5-đồng-bộ-ngưỡng-cảnh-báo)

---

## 1. `sensor-node` → CoreIoT (MQTT telemetry)

**Topic**: `v1/devices/me/telemetry` (chuẩn ThingsBoard, publish từ `firmware/sensor-node`, xem [`CoreiotClient.cpp`](../../firmware/sensor-node/src/CoreiotClient.cpp)).
**Tần suất**: mỗi `COREIOT_PUBLISH_INTERVAL_MS` (~2 giây, 2Hz).
**Định dạng**: JSON phẳng (flat), key nào không có dữ liệu hợp lệ thì **vắng mặt** trong payload (không gửi `null`).

| Field | Kiểu | Đơn vị | Ví dụ | Ghi chú |
| --- | --- | --- | --- | --- |
| `left_front` | float | cm | `85.3` | Khoảng cách đã lọc (Cluster+EMA) của cảm biến S3 (trước-trái). Vắng mặt nếu cảm biến mất tín hiệu hoặc chưa đủ mẫu warm-up. |
| `right_front` | float | cm | `142.0` | Khoảng cách đã lọc của cảm biến S5 (trước-phải). Cùng quy tắc vắng mặt như trên. |

Ví dụ payload thực tế:

```json
{"left_front": 85.3, "right_front": 142.0}
```

**Khoảng trống hiện tại**: kiến trúc phần mềm hỗ trợ tối đa 6 cảm biến (`SENSOR_PINS[]`, `sensor_id_t` phía `waveshare-screen`), nhưng **chỉ 2/6 cảm biến được lắp phần cứng thật** (S3, S5) — xem [`report/README.md` mục 1](../../report/README.md#1-giới-thiệu--mục-tiêu). Do đó các key `front` (S1), `rear` (S2), `left_rear` (S4), `right_rear` (S6) **không bao giờ xuất hiện** trong payload telemetry hiện nay, dù `networkTask` có thể được mở rộng để publish thêm key mới khi lắp thêm cảm biến (xem [`docs/API_GUIDE.md` mục 3.2](../API_GUIDE.md#32-thêmbớt-cảm-biến-trên-sensor-node)).

## 2. CoreIoT Rule-Chain → `waveshare-screen` (MQTT shared attributes)

**Cơ chế**: node `Update Shared Attributes (notifyDevice=true)` trong Rule-Chain ([`cloud/coreiot/rule_chain/supersonic_rule_chain.json`](../../cloud/coreiot/rule_chain/supersonic_rule_chain.json)) đẩy xuống `waveshare-screen` qua **MQTT Shared Attributes** (`SHARED_SCOPE`, không phải telemetry thường) — `notifyDevice=true` khiến thiết bị đang subscribe nhận được ngay lập tức. Bản tin được tạo ra bởi node script JS `Process Ultrasonic & Vehicle Data`, sau đó đổi chủ thể (originator) sang `waveshare-screen` bằng node `Change Originator`. Chi tiết đầy đủ từng node: [`docs/API_GUIDE.md` mục 4](../API_GUIDE.md#4-rule-chain-coreiot--cấu-hình--api-node).

| Field | Kiểu | Giá trị hợp lệ | Ví dụ | Ghi chú |
| --- | --- | --- | --- | --- |
| `vehicle_detected` | boolean | `true` / `false` | `true` | `0 < dist <= 50` (cm), `dist = min(left_front, right_front)` (hoặc giá trị còn lại nếu chỉ 1 key có mặt). |
| `warning_status` | string enum | `"NORMAL"` \| `"WARNING"` \| `"DANGER"` | `"WARNING"` | `"DANGER"` nếu `dist < 20`, `"WARNING"` nếu `vehicle_detected` nhưng chưa tới ngưỡng DANGER, ngược lại `"NORMAL"`. |
| `relay` | string | `"ON"` / `"OFF"` | `"ON"` | Mirror `vehicle_detected` — mô phỏng relay điều khiển ngoài (không có relay vật lý thật ở phiên bản hiện tại). |
| `buzzer` | string | `"ON"` / `"OFF"` | `"ON"` | Mirror `relay` — **chỉ để hiển thị đồng bộ trên dashboard**, còi vật lý thật trên `sensor-node` được điều khiển **cục bộ**, không qua round-trip cloud (giữ độ trễ thấp). |
| `source_device` | string | luôn `"sensor-node"` | `"sensor-node"` | Đánh dấu nguồn gốc bản tin sau khi đổi originator, để `waveshare-screen` phân biệt với các bản tin khác nếu có. |
| `left_front` | float (optional) | cm | `85.3` | Passthrough từ payload gốc — chỉ có mặt nếu key gốc có mặt. |
| `right_front` | float (optional) | cm | `142.0` | Passthrough từ payload gốc — chỉ có mặt nếu key gốc có mặt. |

Ví dụ payload thực tế (`msgType: "POST_ATTRIBUTES_REQUEST"`):

```json
{
  "vehicle_detected": true,
  "warning_status": "WARNING",
  "relay": "ON",
  "buzzer": "ON",
  "source_device": "sensor-node",
  "left_front": 85.3,
  "right_front": 142.0
}
```

## 3. `sensor_model` — struct nội bộ trên `waveshare-screen`

**Không phải payload MQTT** — đây là struct trong bộ nhớ (RAM) trên `waveshare-screen`, nơi dữ liệu từ mục 2 được `coreiot_client`'s data callback (`on_mqtt_data`, parse JSON bằng cJSON) ánh xạ vào thông qua `sensor_model_set_distance()`. Định nghĩa: [`components/sensor_model/include/sensor_model.h`](../../firmware/waveshare-screen/components/sensor_model/include/sensor_model.h). API đầy đủ: [`docs/API_GUIDE.md` mục 2.1](../API_GUIDE.md#21-sensor_model--mô-hình-dữ-liệu-6-cảm-biến-thread-safe).

### `sensor_id_t` — 6 slot cảm biến (khớp thứ tự S1..S6)

| Giá trị enum | Index | Vị trí lắp (compass) | Góc lắp `offset_deg` | Key JSON tương ứng (mục 2) |
| --- | --- | --- | --- | --- |
| `SENSOR_ID_FRONT` | 0 | S1 — trước | 0° | *(chưa publish — xem mục 4)* |
| `SENSOR_ID_REAR` | 1 | S2 — sau | 180° | *(chưa publish)* |
| `SENSOR_ID_LEFT_FRONT` | 2 | S3 — trước-trái | -90° (trái) | `left_front` |
| `SENSOR_ID_LEFT_REAR` | 3 | S4 — sau-trái | -90° (trái) | *(chưa publish)* |
| `SENSOR_ID_RIGHT_FRONT` | 4 | S5 — trước-phải | +90° (phải) | `right_front` |
| `SENSOR_ID_RIGHT_REAR` | 5 | S6 — sau-phải | +90° (phải) | *(chưa publish)* |

Mỗi phần tử (`sensor_reading_t`) gồm: `distance_cm` (uint16_t), `offset_deg` (góc lắp cố định, dùng để vẽ vị trí trên canvas xe 2D), `is_stale` (cờ đánh dấu dữ liệu cũ/chưa cập nhật). Toàn bộ struct 6 phần tử bảo vệ bằng **1 mutex FreeRTOS dùng chung** cho mọi task đọc/ghi (network callback ghi, UI task đọc).

### Ngưỡng phân vùng (zone) — `sensor_model_classify()`

| Zone | Điều kiện | Ý nghĩa hiển thị |
| --- | --- | --- |
| `SENSOR_ZONE_SAFE` | `distance_cm > 100` | An toàn — không cảnh báo. |
| `SENSOR_ZONE_CAUTION` | `30 <= distance_cm <= 100` | Vùng chú ý — hiển thị màu cảnh báo nhẹ trên arc. |
| `SENSOR_ZONE_DANGER` | `distance_cm < 30` | Vùng nguy hiểm — nhấp nháy/màu đỏ trên arc. |

Ngưỡng này **hardcode trong mã nguồn C** (`firmware/waveshare-screen/components/sensor_model/sensor_model.c`), không phải tham số cấu hình — khác và **độc lập** với ngưỡng WARNING/DANGER 50cm/20cm của Rule-Chain/buzzer ở mục 2 (xem mục 5 bên dưới).

## 4. Khoảng trống đã biết: chỉ 2/6 slot `sensor_model` có dữ liệu sống

Kiến trúc phần mềm hỗ trợ đầy đủ 6 cảm biến ở **cả 3 lớp** (mảng `SENSOR_PINS[]` phía `sensor-node`, 6 field `sensor_id_t` phía `sensor_model`, 6 cung `lv_arc` trên UI) — nhưng dữ liệu thật hiện chỉ chảy qua **2 slot**: `SENSOR_ID_LEFT_FRONT` và `SENSOR_ID_RIGHT_FRONT`, vì:

1. Chỉ 2/6 cảm biến vật lý (S3, S5) được lắp trên phần cứng thật (xem [`report/README.md` mục 1](../../report/README.md#1-giới-thiệu--mục-tiêu)).
2. `sensor-node` chỉ publish 2 key JSON (`left_front`/`right_front`) lên MQTT telemetry (mục 1).
3. `coreiot_client` trên `waveshare-screen` chỉ parse 2 key này để gọi `sensor_model_set_distance()`.

Kết quả: 4 slot còn lại (`SENSOR_ID_FRONT`, `SENSOR_ID_REAR`, `SENSOR_ID_LEFT_REAR`, `SENSOR_ID_RIGHT_REAR`) **luôn giữ giá trị mặc định/stale** trên `waveshare-screen` — không phải lỗi, mà là hệ quả trực tiếp của việc chưa lắp đủ phần cứng. **Người đóng góp mới không nên giả định cả 6 cảm biến đều "sống"** khi đọc code UI hay debug dashboard — 4 cung còn lại trên canvas xe sẽ luôn hiển thị trạng thái mặc định cho tới khi có phần cứng bổ sung và cả 3 lớp trên được mở rộng đồng bộ (hướng dẫn thêm cảm biến: [`docs/API_GUIDE.md` mục 3.2](../API_GUIDE.md#32-thêmbớt-cảm-biến-trên-sensor-node) và [mục 4.3](../API_GUIDE.md#43-node-xử-lý-chính--process-ultrasonic--vehicle-data-js-script)).

## 5. Đồng bộ ngưỡng cảnh báo

File này mô tả **hình dạng** dữ liệu (field nào, kiểu gì) — không lặp lại giá trị ngưỡng cụ thể (20cm/50cm cho WARNING/DANGER, hay 30cm/100cm cho SAFE/CAUTION/DANGER ở mục 3), vì các giá trị này tồn tại ở **2 nơi không tự động đồng bộ** (firmware `Config.h` và JS script trên Rule-Chain CoreIoT) và có thể trôi lệch giữa các lần chỉnh sửa. Xem chi tiết đầy đủ, cách sửa đúng cả 2 nơi, và cảnh báo gotcha tại **[`docs/API_GUIDE.md` mục 3.4 — Đổi ngưỡng cảnh báo WARNING/DANGER](../API_GUIDE.md#34-đổi-ngưỡng-cảnh-báo-warningdanger-buzzer--rule-chain--ui)**.
