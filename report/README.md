# Báo cáo Kỹ thuật — Hệ thống Cảnh báo Va chạm bằng Cảm biến Siêu âm JSN-SR04T & ESP-NOW

> Tài liệu này là bản báo cáo **đầy đủ** (kèm nhật ký phát triển, sự cố đã gặp và hạn chế hiện tại) của dự án [`supersonic-sensor-ACLAB`](https://github.com/YdtTran/supersonic-warning-system). Bản báo cáo **trang trọng, chỉ mô tả hệ thống ở trạng thái hiện tại** (không có log/lịch sử) nằm ở [`report.tex`](report.tex) / [`report.pdf`](report.pdf), biên dịch bằng pdflatex (MiKTeX).
>
> ⚠️ **Lưu ý kiến trúc**: hệ thống đã **chuyển từ đường MQTT/CoreIoT sang ESP-NOW trực tiếp** giữa 2 board (xem [mục 5.3](#53-kết-nối-esp-now-đường-truyền-hiện-tại) và [mục 9.9](#99-chuyển-sang-esp-now--gỡ-bỏ-phụ-thuộc-cloud)). Các mục mô tả CoreIoT/Rule-Chain (mục 6.1 và [mục 7](#7-cloud-coreiot-rule-chain)) được giữ lại làm **bối cảnh lịch sử** — code vẫn còn trong cây nguồn nhưng không được gọi trên nhánh hiện tại.
>
> Tài liệu này tập trung vào **kiến trúc, quyết định thiết kế và lịch sử phát triển**. Để tra cứu **API từng thư viện/component** (chữ ký hàm, ví dụ code) và **hướng dẫn cấu hình bằng phần mềm** (đổi Wi-Fi/MQTT, thêm cảm biến, đổi ngưỡng cảnh báo, cấu hình Rule-Chain), xem [`docs/API_GUIDE.md`](../docs/API_GUIDE.md).

---

## Mục lục

1. [Giới thiệu & Mục tiêu](#1-giới-thiệu--mục-tiêu)
2. [Kiến trúc tổng quan](#2-kiến-trúc-tổng-quan)
3. [Phần cứng sử dụng](#3-phần-cứng-sử-dụng)
4. [Công cụ & Framework](#4-công-cụ--framework)
5. [Firmware `sensor-node`](#5-firmware-sensor-node)
6. [Firmware `waveshare-screen`](#6-firmware-waveshare-screen)
7. [Cloud CoreIoT (Rule-Chain)](#7-cloud-coreiot-rule-chain) — *không dùng trên nhánh hiện tại*
8. [Prototype thử nghiệm](#8-prototype-thử-nghiệm)
9. [Nhật ký & lịch sử phát triển](#9-nhật-ký--lịch-sử-phát-triển)
10. [Hạn chế & việc cần làm thêm](#10-hạn-chế--việc-cần-làm-thêm)
11. [Ảnh minh hoạ & đề xuất bổ sung](#11-ảnh-minh-hoạ--đề-xuất-bổ-sung)

---

## 1. Giới thiệu & Mục tiêu

Dự án xây dựng hệ thống nhúng phát hiện vật cản/xe cộ ở khoảng cách gần bằng **cảm biến siêu âm chống nước JSN-SR04T**, gắn trên vi điều khiển **ESP32-S3 (board Yolo:Uno)**. Khoảng cách đã lọc được gửi **trực tiếp qua ESP-NOW** (liên kết Wi-Fi cục bộ giữa 2 board, không qua AP/router/cloud) sang **màn hình cảm ứng Waveshare ESP32-S3 Touch LCD 7"**, nơi ngưỡng cảnh báo được **tự đánh giá cục bộ trên thiết bị** và hiển thị trực quan (dashboard va chạm kiểu "no-zone" quanh xe); còi cảnh báo vật lý được điều khiển ngay trên `sensor-node`.

Phiên bản trước đó định tuyến dữ liệu qua **CoreIoT (ThingsBoard)** bằng MQTT + **Rule-Chain** — kiến trúc này đã được thay thế để loại bỏ phụ thuộc hạ tầng mạng và giảm độ trễ cảnh báo (xem [mục 9.9](#99-chuyển-sang-esp-now--gỡ-bỏ-phụ-thuộc-cloud)); mã nguồn CoreIoT vẫn được giữ lại trong cây nguồn để có thể khôi phục.

Ý tưởng gốc (xem `architecture.png`) là hệ thống 6 cảm biến bao quanh toàn bộ xe (trước/sau/4 góc) kết hợp cảnh báo cho người đi đường. **Hệ thống hiện tại đã triển khai đủ 6/6 cảm biến** (S1 – trước, S2 – sau, S3/S4 – hông trái, S5/S6 – hông phải) trên phần cứng thật; phần cảnh báo cho người đi đường bên ngoài xe **vẫn đang chờ bổ sung phần cứng**, không phải là thu hẹp phạm vi dự án.

## 2. Kiến trúc tổng quan

```text
 ┌────────────────────────┐
 │   Cảm biến JSN-SR04T   │  x6 (S1 trước, S2 sau, S3/S4 hông trái, S5/S6 hông phải)
 └───────────┬────────────┘
             │ Echo / Trig GPIO
             ▼
 ┌────────────────────────┐
 │  ESP32-S3 Sensor Node  │  firmware/sensor-node — đo, lọc nhiễu, còi báo cục bộ
 └───────────┬────────────┘
             │ ESP-NOW trực tiếp (channel 1 cố định, 30 bytes/gói, 2Hz)
             │ KHÔNG qua Wi-Fi AP / router / cloud
             ▼
 ┌────────────────────────┐
 │ Waveshare Screen Node  │  firmware/waveshare-screen — tự đánh giá hazard cục bộ,
 └────────────────────────┘  dashboard LVGL 800x480
```

Kiến trúc cũ (**không còn hoạt động trên nhánh này**, giữ lại trong cây nguồn):

```text
 ESP32-S3 Sensor Node → Wi-Fi MQTT → CoreIoT (app.coreiot.io)
   → Rule-Chain tính ngưỡng → MQTT Shared Attributes → Waveshare Screen Node
```

Sơ đồ ý tưởng gốc (6 cảm biến bao quanh xe kèm cảnh báo cho người đi đường — phần cảm biến **đã triển khai đủ**, phần cảnh báo ngoài xe chưa):

![Kiến trúc ý tưởng ban đầu](./architecture.png)

## 3. Phần cứng sử dụng

| Thành phần | Mô tả |
|---|---|
| MCU | **Yolo:Uno** — board phát triển dựa trên **ESP32-S3-WROOM-1** (Dual-Core 240MHz, Wi-Fi/BLE, PSRAM Octal). Pinout đầy đủ bên dưới. |
| Cảm biến khoảng cách | **JSN-SR04T V3** — cảm biến siêu âm chống nước, tách rời đầu dò và board mạch, chạy ở **Mode 0 (mặc định)**: MCU phát xung Trig, đọc trực tiếp độ rộng xung Echo qua GPIO (không qua UART, không cần chỉnh jumper). Dòng JSN-SR04T còn hỗ trợ các Mode khác (vd tự động đo và trả khoảng cách qua UART) chọn bằng điện trở `R27` trên board (xem ảnh minh hoạ bên dưới). |
| Màn hình hiển thị | **Waveshare ESP32-S3 Touch LCD 7"** — panel RGB 800×480, cảm ứng dung kháng **GT911** (I2C, 400kHz Fast-mode), IO-expander **CH422G** (I2C, địa chỉ `0x24`/`0x38`) điều khiển backlight, reset cảm ứng, chip-select thẻ SD và MUX CAN. |
| Cảnh báo cục bộ | Còi buzzer GPIO11 gắn trực tiếp trên `sensor-node` — phản hồi ngay lập tức không qua round-trip cloud. |

![Pinout Yolo:Uno (ESP32-S3)](./image.png)

![Board JSN-SR04T — khoanh vùng R27 chọn Mode hoạt động](./sr04t.png)

*Ảnh minh hoạ chung cho dòng sản phẩm JSN-SR04T: điện trở `R27` (khoanh hồng) là jumper chọn giữa Mode UART tự động (module tự đo và trả khoảng cách qua UART) và Mode 3 (vi điều khiển tự phát xung Trig, đọc độ rộng xung Echo). Cảm biến thực tế trên `sensor-node` chạy ở **Mode 0 (mặc định)**, không cần chỉnh jumper. Việc debug Mode qua `R27` mô tả ở [mục 9.3](#93-prototype-pulse-read-prototype--debug-mode-cảm-biến) áp dụng cho cảm biến **SR04M-2** dùng trong prototype `pulse-read-prototype`, không phải JSN-SR04T V3 trên `sensor-node`.*

Ảnh chụp dashboard chạy trên phần cứng thật:

![Dashboard chạy trên Waveshare LCD 7"](./dashboard-screen-img.jpg)

## 4. Công cụ & Framework

| Hạng mục | Lựa chọn | Lý do |
|---|---|---|
| Build system | **PlatformIO** (`pio run -e yolo_uno`) cho cả 2 firmware | Layout thống nhất (`src/`, `boards/`, `platformio.ini`, `build_and_flash.bat`), một toolchain quản lý cả 2 dự án khác framework. |
| `sensor-node` | `framework = arduino` | Tái sử dụng hệ sinh thái thư viện Arduino-ESP32 sẵn có (`WiFi.h`, `esp_now.h`) — đủ dùng cho tác vụ đo/lọc/gửi ESP-NOW, không cần driver màn hình phức tạp. |
| `waveshare-screen` | `framework = espidf` **thuần** (không Arduino) | **Quyết định kiến trúc quan trọng**: driver màn hình/cảm ứng hiện đại (`esp_lvgl_adapter`, `esp_lcd_touch_gt911` bản mới, API I2C `i2c_master.h`, LVGL 9.1, `esp_lcd_panel_rgb` với `num_fbs`/bounce-buffer) đòi hỏi **ESP-IDF ≥ 5.5**, trong khi tổ hợp `framework = arduino, espidf` của PlatformIO chỉ cấp **ESP-IDF 4.4.7** — không tương thích. Xem chi tiết quá trình quyết định ở [mục 9](#9-nhật-ký--lịch-sử-phát-triển). |
| Đồ hoạ UI | **LVGL 9.1** + `espressif/esp_lvgl_adapter` | Thư viện GUI nhúng mã nguồn mở phổ biến nhất cho ESP32, adapter chính thức của Espressif quản lý sẵn vòng lặp render + khoá luồng an toàn. |
| Liên kết 2 board | **ESP-NOW** (lớp 2, trên nền Wi-Fi radio) | Không cần AP/router/broker → hệ thống chạy được cả khi không có mạng, độ trễ mỗi gói dưới ~10ms, phù hợp cảnh báo va chạm thời gian thực. |
| Cloud IoT *(không còn dùng)* | **CoreIoT (ThingsBoard)** | Nền tảng MQTT-native miễn phí, Rule-Chain kéo-thả xử lý ngưỡng không cần backend riêng. Đã thay bằng ESP-NOW + đánh giá hazard cục bộ (mục 9.9). |
| Biên dịch báo cáo | **MiKTeX + pdflatex** (đã cài sẵn) | Dùng để build `report.tex` → `report.pdf`. |

## 5. Firmware `sensor-node`

### 5.1 Kiến trúc tác vụ FreeRTOS

[`firmware/sensor-node/src/main.cpp`](https://github.com/YdtTran/supersonic-warning-system/blob/main/firmware/sensor-node/src/main.cpp) tạo 4 task FreeRTOS trong `setup()`:

| Task | Stack | Priority | Core | Vai trò |
|---|---|---|---|---|
| `SensorTask` | 4096 | 2 | 1 | Trigger + đọc tuần tự từng cảm biến (tránh nhiễu âm học giữa các cảm biến), đưa qua bộ lọc, ghi kết quả vào `SharedState` (mutex). |
| `AppTask` | 2048 | 1 | 1 | Ví dụ tiêu thụ dữ liệu đã lọc song song, độc lập chu kỳ đo (bật LED khi có vật gần). |
| `NetworkTask` | 4096 | 1 | 0 | Đóng gói `SharedState` thành `espnow_sensor_msg_t` và gửi ESP-NOW tới `waveshare-screen` mỗi `ESPNOW_SEND_INTERVAL_MS` (500ms), tách khỏi core 1 để không ảnh hưởng timing đo (ràng buộc bằng microsecond). |
| `BuzzerTask` | 2048 | 1 | 0 | Đọc khoảng cách gần nhất trong `SharedState`, điều khiển còi vật lý GPIO11 theo ngưỡng WARNING/DANGER. |

**Lịch sử**: `buzzerTask()` từng được viết đầy đủ logic nhưng **thiếu lời gọi `xTaskCreatePinnedToCore` trong `setup()`**, nên còi cảnh báo chưa bao giờ thực sự kêu dù code trông hoàn chỉnh. Đã bổ sung — xem [`docs/logs/SENSOR_NODE_GPIO47_48_PSRAM_LOG.md`](../docs/logs/SENSOR_NODE_GPIO47_48_PSRAM_LOG.md).

> API chi tiết + ví dụ code của `UltrasonicSensor`, `DistanceFilter`, `SharedState`, `CoreiotClient`: [`docs/API_GUIDE.md` mục 1](../docs/API_GUIDE.md#1-firmwaresensor-node-arduino--thư-viện-đo--lọc-cảm-biến).

### 5.2 Bộ lọc khoảng cách (Cluster + EMA)

Tham số cấu hình đầy đủ tại [`firmware/sensor-node/include/Config.h`](https://github.com/YdtTran/supersonic-warning-system/blob/main/firmware/sensor-node/include/Config.h):

| Tham số | Giá trị | Ý nghĩa |
|---|---|---|
| `HISTORY_SIZE` | 9 | Số mẫu RAW gần nhất lưu lại để phân cụm. |
| `MIN_SAMPLES_TO_FILTER` | 5 | Cần tối thiểu 5 mẫu mới bắt đầu xuất kết quả. |
| `MIN_CLUSTER_SIZE` | 5 | Một cụm hợp lệ phải chiếm ít nhất 5/9 mẫu gần nhất. |
| `BASE_CLUSTER_TOLERANCE_CM` / `CLUSTER_TOLERANCE_RATIO` | 8 cm / 0.08 | Dung sai để 2 mẫu được xem là cùng cụm, tăng theo khoảng cách (nhiễu tỉ lệ thuận với khoảng cách đo). |
| `EMA_ALPHA` | 0.30 | Hệ số làm mượt kết quả cuối (EMA — Exponential Moving Average). |
| `MIN_JUMP_THRESHOLD_CM` / `JUMP_THRESHOLD_RATIO` | 30 cm / 0.25 | Ngưỡng xem là "bước nhảy lớn" (vật cản mới xuất hiện/biến mất). |
| `JUMP_CONFIRM_COUNT` | 3 | Bước nhảy phải được xác nhận liên tiếp 3 lần trước khi chấp nhận, tránh nhiễu tức thời. |
| `RESET_AFTER_INVALID` | 15 | Sau 15 lần đọc lỗi liên tiếp, reset bộ lọc và báo mất tín hiệu. |

Lý do dùng cluster+EMA thay vì lọc trung vị đơn giản: cảm biến siêu âm giá rẻ (JSN-SR04T) dễ có outlier do phản xạ đa hướng — phân cụm loại outlier hiệu quả hơn, EMA làm mượt kết quả hiển thị mà vẫn phản ứng nhanh với thay đổi thật (xác nhận bằng "jump" logic).

> Cách tinh chỉnh các tham số này (làm mượt hơn/phản ứng nhanh hơn, thêm cảm biến, đổi Wi-Fi/MQTT) không cần sửa logic: [`docs/API_GUIDE.md` mục 3](../docs/API_GUIDE.md#3-cấu-hình-bằng-phần-mềm--không-cần-sửa-code-logic).

### 5.3 Kết nối ESP-NOW (đường truyền hiện tại)

[`firmware/sensor-node/src/EspNowClient.cpp`](https://github.com/YdtTran/supersonic-warning-system/blob/main/firmware/sensor-node/src/EspNowClient.cpp) gửi trực tiếp tới MAC của `waveshare-screen` bằng `esp_now_send()`:

- `WiFi.mode(WIFI_STA)` chỉ để **mở radio Wi-Fi**, không gọi `WiFi.begin()` — không kết nối AP nào, không broker, không phụ thuộc hạ tầng mạng.
- Channel cố định **1** (phải khớp 2 board); MAC đích khai báo ở `ESPNOW_PEER_MAC` (`EspNowConfig.h`).
- Payload là **struct nhị phân packed** `espnow_sensor_msg_t` (`float[6]` + `uint8_t[6]` = **30 bytes**, không phải JSON), luôn mang đủ 6 slot; `valid[i]=0` biểu thị slot đang mất tín hiệu.
- Tần suất gửi 500ms (2Hz), tách biệt với chu kỳ đo/lọc cục bộ 100ms — bộ lọc vẫn phản ứng nhanh cho cảnh báo, chỉ giảm tải đường truyền.

So với đường MQTT/CoreIoT trước đây, ESP-NOW bỏ hoàn toàn các bước AP association/DHCP/broker nên độ trễ mỗi gói xuống dưới ~10ms và hệ thống **hoạt động được cả khi không có mạng** — đúng yêu cầu của một hệ cảnh báo va chạm gắn trên xe di chuyển.

> Thông số kỹ thuật đầy đủ (tầm hoạt động, giới hạn payload/peer, ACK & độ tin cậy, bảo mật): [`docs/architecture/ESPNOW_NETWORK.md`](../docs/architecture/ESPNOW_NETWORK.md).

### 5.4 Buzzer cảnh báo cục bộ

Ngưỡng nằm hoàn toàn trong [`Config.h`](https://github.com/YdtTran/supersonic-warning-system/blob/main/firmware/sensor-node/include/Config.h) trên `sensor-node` (không còn bản sao trên Rule-Chain vì đường cloud đã ngừng dùng — không còn nguy cơ trôi lệch ngưỡng giữa 2 nơi):

- **WARNING** (20–50 cm): kêu 1 lần mỗi 3 giây.
- **DANGER** (< 20 cm): kêu 1 lần mỗi 1 giây.
- Độ dài mỗi tiếng kêu: 120 ms, không chặn (non-blocking, dùng `millis()`).
- Chân còi: **GPIO 11** (trước đây GPIO 48 — phải đổi vì trùng chân Echo của cảm biến REAR khi mở rộng lên 6 cảm biến).

## 6. Firmware `waveshare-screen`

### 6.1 [`coreiot_client`](https://github.com/YdtTran/supersonic-warning-system/tree/main/firmware/waveshare-screen/components/coreiot_client) — kết nối mạng thuần ESP-IDF *(không dùng trên nhánh hiện tại)*

> **Không được gọi từ `src/main.c` trên nhánh này** — đường nhận dữ liệu hiện tại là ESP-NOW (xem [mục 6.5](#65-nhận-dữ-liệu-esp-now--đánh-giá-hazard-cục-bộ-đường-truyền-hiện-tại)). Component vẫn còn trong cây nguồn để khôi phục MQTT sau này; phần mô tả dưới đây giữ nguyên làm tài liệu tham chiếu.

Không dùng Arduino core nên không có `WiFi.h`/`PubSubClient` — thay bằng API ESP-IDF gốc:

- `esp_wifi.h`, `esp_event.h`, `esp_netif.h`, `mqtt_client.h` (component **esp-mqtt**), `nvs_flash.h`.
- Chuỗi khởi tạo: `nvs_flash_init` → `esp_netif_init` → `esp_event_loop_create_default` → `esp_netif_create_default_wifi_sta` → `esp_wifi_init/set_mode/set_config/start`, sau đó `esp_mqtt_client_init` + `esp_mqtt_client_register_event`.
- Khi `MQTT_EVENT_CONNECTED`: subscribe topic telemetry, `v1/devices/me/attributes`, và `v1/devices/me/attributes/response/+` (đồng bộ trạng thái ban đầu), rồi publish một bản tin "reboot event".
- Dữ liệu đến được chuyển ra ngoài qua callback thô (topic/payload con trỏ) — việc parse JSON nằm ở lớp gọi ([`firmware/waveshare-screen/src/main.c`](https://github.com/YdtTran/supersonic-warning-system/blob/main/firmware/waveshare-screen/src/main.c)), giữ `coreiot_client` framework-agnostic.

### 6.2 [`sensor_model`](https://github.com/YdtTran/supersonic-warning-system/tree/main/firmware/waveshare-screen/components/sensor_model) — mô hình dữ liệu dùng chung

Struct 6 cảm biến (`SENSOR_MODEL_COUNT`), mỗi phần tử gồm `distance_cm`, góc lắp `offset_deg` cố định (trước=0°, sau=180°, 2 bên=±90°), cờ `is_stale`; bảo vệ bằng 1 mutex FreeRTOS dùng chung cho mọi task đọc/ghi. Hàm `sensor_model_classify()` phân loại khoảng cách thành 3 vùng: **SAFE** (>100cm) / **CAUTION** (30–100cm) / **DANGER** (<30cm).

### 6.3 [`ui_dashboard`](https://github.com/YdtTran/supersonic-warning-system/tree/main/firmware/waveshare-screen/components/ui_dashboard) — giao diện LVGL 9.1

Widget sử dụng: `lv_arc_*` (6 cung "chùm sóng" quanh sơ đồ xe), `lv_label_set_text_fmt` (số liệu động), `lv_anim_*` (hiệu ứng nhấp nháy vùng nguy hiểm), `lv_timer_create` (refresh thông tin hệ thống mỗi 2s), layout flex (`lv_obj_set_flex_flow/align`). Tab COLLISION/SYSTEM được tự dựng bằng 2 nút + `LV_OBJ_FLAG_HIDDEN` (không dùng `lv_tabview` có sẵn của LVGL, để tuỳ biến giao diện dễ hơn).

**Cơ chế khoá luồng** `esp_lv_adapter_lock()`/`unlock()`: vì callback nhận dữ liệu mạng (hiện là `on_data_recv` của ESP-NOW; trước đây là callback Wi-Fi/MQTT) chạy trên task khác với task LVGL (esp_lv_adapter chạy LVGL trong task riêng, stack 12KB trong PSRAM), mọi thao tác cập nhật UI từ callback đó phải khoá trước khi gọi API LVGL. Ban đầu dùng timeout vô hạn (`-1`) — tiềm ẩn treo toàn hệ thống nếu task LVGL bị kẹt; đã sửa thành `LV_LOCK_TIMEOUT_TICKS = pdMS_TO_TICKS(100)` cho mọi lần khoá từ network callback. Riêng lần khoá lúc khởi tạo UI trong `app_main()` (trước khi task mạng chạy, không có tranh chấp) vẫn giữ `-1`.

> API chi tiết + ví dụ code của `sensor_model`, `coreiot_client`, `ui_dashboard`: [`docs/API_GUIDE.md` mục 2](../docs/API_GUIDE.md#2-firmwarewaveshare-screen-esp-idf--component-dashboard).

### 6.4 BSP — driver phần cứng màn hình

[`firmware/waveshare-screen/src/bsp/waveshare_rgb_lcd_port.c`](https://github.com/YdtTran/supersonic-warning-system/blob/main/firmware/waveshare-screen/src/bsp/waveshare_rgb_lcd_port.c):

- Panel RGB: `esp_lcd_rgb_panel_config_t` + `esp_lcd_new_rgb_panel()` + `esp_lcd_panel_init()` — độ phân giải 800×480, RGB565, xung clock pixel 16MHz, framebuffer cấp phát trong **PSRAM** (`flags.fb_in_psram=1`), số lượng framebuffer lấy từ `esp_lv_adapter_get_required_frame_buffer_count()`.
- I2C bus dùng API mới `driver/i2c_master.h` (`i2c_new_master_bus`, `i2c_master_bus_add_device`, `i2c_master_transmit`) — dùng chung cho cả CH422G lẫn reset GT911.
- **CH422G**: không có driver ESP-IDF riêng, điều khiển bằng ghi thanh ghi thô qua địa chỉ I2C `0x24`/`0x38` (backlight, reset cảm ứng, CS thẻ SD, MUX CAN).
- **GT911**: dùng driver chính thức `esp_lcd_touch_gt911.h` — `esp_lcd_new_panel_io_i2c()` + `ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG()` + `esp_lcd_touch_new_i2c_gt911()`.

### 6.5 Nhận dữ liệu ESP-NOW & đánh giá hazard cục bộ (đường truyền hiện tại)

Logic nhận ESP-NOW nằm trực tiếp trong [`src/main.c`](https://github.com/YdtTran/supersonic-warning-system/blob/main/firmware/waveshare-screen/src/main.c), **không tách thành component riêng** — `esp_now` là API hệ thống của ESP-IDF, không cần lớp bọc như `coreiot_client`:

- `networkTask` khởi tạo Wi-Fi STA "trần" (không kết nối AP), cố định channel 1, rồi đăng ký `esp_now_register_recv_cb(on_data_recv)`.
- `on_data_recv()` kiểm tra đúng kích thước gói, sau đó với mỗi slot gọi `ui_dashboard_update_sensor()` (khi `valid=1`) hoặc `ui_dashboard_clear_sensor()` (khi `valid=0`), toàn bộ trong `esp_lv_adapter_lock(100ms)` vì callback chạy ngoài task LVGL.
- `evaluate_hazard()` trong `ui_dashboard.c` **tự tính banner "OVERALL"** từ 6 slot, bỏ qua slot `is_stale` — thay cho `warning_status`/`relay` mà Rule-Chain từng tính trên cloud.
- **Watchdog liên kết**: `esp_timer` chu kỳ 1s kiểm tra thời gian từ gói cuối; quá 1.5s không nhận được gì thì badge header chuyển "ESP-NOW: NO LINK". Đây là phần bù cho việc ESP-NOW không có heartbeat sẵn ở tầng giao thức.

## 7. Cloud CoreIoT (Rule-Chain)

> Toàn bộ mục này mô tả kiến trúc **trước khi chuyển sang ESP-NOW** ([mục 9.9](#99-chuyển-sang-esp-now--gỡ-bỏ-phụ-thuộc-cloud)). Rule-Chain không còn nằm trên đường dữ liệu; giữ lại làm tài liệu tham chiếu và để khôi phục nếu cần publish MQTT song song sau này.

![Rule-Chain CoreIoT](./rule-chain.png)

Rule-Chain [`cloud/coreiot/rule_chain/supersonic_rule_chain.json`](https://github.com/YdtTran/supersonic-warning-system/blob/main/cloud/coreiot/rule_chain/supersonic_rule_chain.json) xử lý bản tin telemetry từ `sensor-node`:

1. **Message Type Switch** → tách luồng "Post telemetry" (lưu timeseries thô) và luồng xử lý chính.
2. **Process Ultrasonic & Vehicle Data** (script JS, `TbTransformMsgNode`):
   - `dist = min(left_front, right_front)` (hoặc giá trị còn lại nếu chỉ 1 cảm biến có dữ liệu).
   - `vehicle_detected = 0 < dist <= 50 cm`.
   - `warning_status`: `"DANGER"` nếu `dist < 20cm`, `"WARNING"` nếu phát hiện nhưng chưa tới ngưỡng nguy hiểm, ngược lại `"NORMAL"`.
   - `relay`: `"ON"/"OFF"` theo `vehicle_detected` (mô phỏng relay điều khiển ngoài).
   - `buzzer`: mirror `relay` — **chỉ để hiển thị đồng bộ trên màn hình**, còi vật lý thật được điều khiển cục bộ ngay trên `sensor-node` (không round-trip qua cloud) để giữ độ trễ thấp.
3. **Change Originator to waveshare-screen** — đổi chủ thể bản tin sang thiết bị màn hình.
4. **Update Shared Attributes** (`SHARED_SCOPE`, `notifyDevice=true`) — đẩy dữ liệu đã xử lý xuống `waveshare-screen` qua MQTT shared-attributes, đồng thời lưu lịch sử timeseries riêng cho thiết bị này.

Lý do chọn CoreIoT: nền tảng ThingsBoard mã nguồn mở/miễn phí, hỗ trợ MQTT gốc, Rule-Chain kéo-thả cho phép viết logic ngưỡng bằng JavaScript ngay trên UI mà không cần triển khai backend riêng, phù hợp quy mô dự án học thuật/prototype.

> Chi tiết từng node, script JS đầy đủ, cách sửa ngưỡng cảnh báo và lưu ý khi import/deploy sang tenant CoreIoT khác: [`docs/API_GUIDE.md` mục 4](../docs/API_GUIDE.md#4-rule-chain-coreiot--cấu-hình--api-node).

## 8. Prototype thử nghiệm

Prototype [`pulse-read-prototype`](https://github.com/YdtTran/supersonic-warning-system/tree/main/prototypes/pulse-read-prototype): đọc trực tiếp xung Trig/Echo qua GPIO trên cảm biến **SR04M-2** (đặt ở **Mode 3** qua jumper `R27`) — cùng kỹ thuật đo time-of-flight trực tiếp mà `sensor-node` áp dụng cho JSN-SR04T V3 (**Mode 0 mặc định**, không cần chỉnh jumper). Prototype này dùng để xác nhận kỹ thuật đo trước khi áp dụng vào `sensor-node`. Quá trình debug thực tế xem [mục 9](#9-nhật-ký--lịch-sử-phát-triển).

## 9. Nhật ký & lịch sử phát triển

### 9.1 Tái cấu trúc thư mục & dashboard va chạm 6-cảm-biến (kiểu "no-zone")

Tái cấu trúc thư mục gốc (`sensor-node/waveshare-screen` → `firmware/`, `coreiot/` → `cloud/coreiot/`, `sub/` → `reference/`, gộp version logs vào `docs/logs/`, dùng `git mv` giữ lịch sử). Viết lại `waveshare-screen` thành 3 component `sensor_model`/`coreiot_client`/`ui_dashboard`, dashboard va chạm 6 cảm biến kiểu "no-zone" của xe tải, tab COLLISION/SYSTEM. Build + flash + verify qua serial thành công. Sửa layout cảm biến theo đúng datasheet (75°, 20cm–6m) và fix lỗi canvas bị lệch tâm (thiếu `pad_column=0`).

### 9.2 Publish 2 cảm biến & sự cố regression

Thêm publish MQTT S3(left_front)/S5(right_front) lên CoreIoT, cập nhật rule chain. Trong quá trình debug Wi-Fi (đổi SSID `ACLAB` → `HCMUT-MEETING`), phát hiện và fix một regression: `SENSOR_COUNT=1` khiến mất dữ liệu S5. Merge [`feature/hardware-uart-gpio44`](https://github.com/YdtTran/supersonic-warning-system/tree/feature/hardware-uart-gpio44) → `main`.

### 9.3 Prototype `pulse-read-prototype` — debug mode cảm biến

Đọc xung trig/echo trực tiếp qua GPIO (SR04M-2 Mode 3) thay vì UART. Quá trình debug khá vất vả: ban đầu tưởng board GT911 bị treo, hoá ra pin-mapping đúng nhưng **module cảm biến đang ở sai Mode** (cần chỉnh jumper `R27` — xem ảnh mục 3). Sau khi sửa, hệ thống chạy rất ổn định và ít nhiễu hơn UART vì đo time-of-flight trực tiếp thay vì giải mã khung UART.

### 9.4 Quyết định kiến trúc: Arduino hybrid thất bại → ESP-IDF thuần

Yêu cầu refactor toàn bộ `waveshare-screen` sang layout PlatformIO trên nhánh `refactor/arduino` (đã merge trực tiếp vào `main` qua commit [`2546d9f`](https://github.com/YdtTran/supersonic-warning-system/commit/2546d9f), nhánh đã bị xoá sau đó). Thử hybrid `framework = arduino, espidf` trước — **thất bại thật sự**: PlatformIO chỉ cấp ESP-IDF 4.4.7 cho tổ hợp có Arduino, trong khi LVGL 9.1/`esp_lvgl_adapter`/GT911 mới cần IDF ≥5.5 (lỗi version-solving của Component Manager, xem [`docs/logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md`](https://github.com/YdtTran/supersonic-warning-system/blob/main/docs/logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md)). Đã đổi hướng dùng `framework = espidf` thuần (không Arduino thật sự), chỉ đổi sang layout PlatformIO (`src/`, `boards/`, `platformio.ini`) để khớp cách tổ chức các project khác trong repo. **Lưu ý:** tên nhánh `refactor/arduino` không phản ánh đúng thực tế — code vẫn là ESP-IDF thuần, không phải Arduino. PlatformIO's ESP-IDF builder cũng đòi hỏi thư mục `src/` tồn tại ở gốc project bất kể dùng Arduino hay không — nên `main/` đã đổi tên thành `src/`.

Kết quả build: `sensor-node` build sạch không đổi (2.37s). `waveshare-screen` build thành công sau khi chuyển framework (544.92s lần đầu — tải toolchain + ESP-IDF 6.0.1 + managed_components, ~264MB+). Tại thời điểm ghi log, **chưa flash/monitor trên phần cứng thật** trong môi trường thực hiện thay đổi — cần người có phần cứng xác nhận trước khi merge (ghi chú lịch sử; theo timeline làm việc thực tế sau đó, việc build đã được xác nhận trên phần cứng thật khi test buzzer — xem mục 9.5-9.6).

### 9.5 Sửa lỗi treo hệ thống tiềm ẩn (lock timeout)

Review code tìm bug tiềm ẩn gây treo hệ thống: phát hiện `esp_lv_adapter_lock(-1)` (chờ vô hạn) trong các callback Wi-Fi/MQTT chạy trên task khác task LVGL → có thể deadlock toàn hệ thống nếu task LVGL bị kẹt. Đã fix thành timeout 100ms, verify qua 15s log serial.

### 9.6 Tune `sensor-node` cho ứng dụng cảnh báo va chạm thực tế

Tăng `MAX_DISTANCE_CM` 450→500cm, `ECHO_TIMEOUT_US` 40ms, tắt bớt `Serial.printf` trong vòng đo tốc độ cao để giảm trễ I/O. Test 2 kịch bản thực tế (vật ở 30–40cm và soi trần ~1.5–2m) đều ổn định 100% OK, không REJECT.

### 9.7 Tính năng buzzer

Rule-chain cloud emit thêm field `buzzer` đồng bộ với `relay`; `sensor-node` điều khiển buzzer vật lý qua GPIO11 cục bộ (không qua round-trip cloud, để phản hồi realtime) — 3s/lần khi WARNING, 1s/lần khi DANGER; `waveshare-screen` hiển thị thêm dòng `BUZZER: ON/OFF`.

### 9.8 Bổ sung UI dashboard (SSID + thông tin hệ thống)

Hiện SSID Wi-Fi đang kết nối, và tab SYSTEM đầy đủ thông tin key CoreIoT (đã che token), firmware version, IDF version, flash/heap/uptime.

### 9.9 Chuyển sang ESP-NOW & gỡ bỏ phụ thuộc cloud

Thay toàn bộ đường `sensor-node → Wi-Fi/MQTT → CoreIoT → MQTT shared attributes → waveshare-screen` bằng **ESP-NOW trực tiếp giữa 2 board**. Lý do: một hệ cảnh báo va chạm gắn trên xe không nên phụ thuộc vào sóng Wi-Fi AP/Internet, và round-trip qua cloud thêm độ trễ không cần thiết cho tình huống cần phản hồi tức thì.

Ràng buộc kỹ thuật đáng chú ý: ESP-NOW và Wi-Fi STA dùng chung radio, nên **không thể vừa kết nối AP vừa giữ ESP-NOW ổn định** nếu channel của AP khác channel ESP-NOW. Vì vậy `sensor-node` **ngắt hẳn** kết nối MQTT/CoreIoT trên nhánh này thay vì chạy song song. Hệ quả kéo theo: `waveshare-screen` phải **tự đánh giá hazard cục bộ** (`evaluate_hazard()`) thay cho `warning_status`/`relay` do Rule-Chain tính; đổi lại, ngưỡng cảnh báo giờ chỉ tồn tại một nơi duy nhất nên không còn nguy cơ trôi lệch giữa firmware và cloud.

Vì 2 project dùng build system khác nhau (PlatformIO/Arduino vs ESP-IDF) và không share include path, struct `espnow_sensor_msg_t` phải được khai báo **trùng khớp thủ công** ở cả 2 phía — [`docs/architecture/ESPNOW_NETWORK.md`](../docs/architecture/ESPNOW_NETWORK.md) là nguồn thông tin dùng chung duy nhất cho MAC/channel/schema.

### 9.10 Mở rộng đủ 6 cảm biến & sự cố GPIO 47/48 bị PSRAM chiếm dụng

Khi lắp nốt 3 cảm biến còn lại (L REAR, R REAR, REAR), cảm biến REAR đấu ở GPIO 47/48 **không bao giờ trả về dữ liệu** dù đèn tín hiệu trên module vẫn nháy bình thường. Quá trình chẩn đoán đi qua 3 lớp nguyên nhân chồng lên nhau:

1. `BUZZER_PIN` khi đó là **GPIO 48** — trùng thẳng chân Echo của cảm biến mới. Đã đổi buzzer sang GPIO 11.
2. `SENSOR_ESPNOW_SLOT[]` bị gán **lệch thứ tự** so với `SENSOR_PINS[]`, khiến dữ liệu hiện sai nhãn cảm biến trên dashboard — lỗi này **không gây lỗi build** nên rất khó phát hiện nếu không đọc kỹ.
3. Nguyên nhân gốc: log serial cho thấy cảm biến đó **luôn `Pulse: 0 us`** (100% timeout, ISR chưa từng chạy), trong khi các cảm biến khác vẫn nhận Echo bình thường. `esptool flash_id` xác nhận chip là bản **Embedded PSRAM 8MB** — GPIO 47/48 bị chip dùng nội bộ làm clock vi sai cho PSRAM, **không hoạt động được như GPIO thường** bất kể firmware cấu hình gì.

Đã chuyển cảm biến REAR sang GPIO 3/4, và bổ sung bảng `RESERVED_PINS[]` + cảnh báo lúc boot trong `sensorTask` để loại lỗi này được phát hiện ngay từ log khởi động thay vì phải lặp lại toàn bộ quy trình chẩn đoán. Nhật ký đầy đủ: [`docs/logs/SENSOR_NODE_GPIO47_48_PSRAM_LOG.md`](../docs/logs/SENSOR_NODE_GPIO47_48_PSRAM_LOG.md).

### 9.11 Phát hiện `buzzerTask` chưa bao giờ chạy

Khi rà soát lại toàn bộ project để đối chiếu tài liệu với code, phát hiện `buzzerTask()` có **logic đầy đủ** và biến handle đã khai báo, nhưng `setup()` **chỉ gọi `xTaskCreatePinnedToCore()` cho 3 task** — thiếu hẳn lời gọi cho `buzzerTask`. Task chưa từng được đưa vào scheduler, nên còi vật lý **chưa bao giờ thực sự kêu** kể từ khi tính năng được viết (mục 9.7), kể cả trước khi đổi chân GPIO.

Trình biên dịch không bắt được vì hàm vẫn được "dùng" gián tiếp qua khai báo handle nên không kích hoạt `-Wunused-function`, và mọi tài liệu đều mô tả tính năng như đang hoạt động. Bản báo cáo này (mục 10 phiên bản trước) từng ghi nghi vấn **"cần xác minh `buzzerTask` được khởi tạo hay chưa"** — nghi ngờ đúng nhưng chưa được kiểm chứng cho tới lần rà soát này. Đã bổ sung lời gọi tạo task (core 0, priority 1, stack 2048).

## 10. Hạn chế & việc cần làm thêm

**Đã hoàn thành** (giữ lại để đối chiếu với các bản báo cáo trước):

- ~~Mở rộng đủ 6 cảm biến~~ — cả 6/6 cảm biến (S1..S6) đã lắp phần cứng và khai báo trong `SENSOR_PINS[]`/`SENSOR_ESPNOW_SLOT[]` (mục 9.10).
- ~~Xác minh `buzzerTask` được khởi tạo~~ — đã xác minh: **không** được khởi tạo, và đã sửa (mục 9.11).

**Còn tồn đọng:**

- **Chưa kiểm chứng tầm hoạt động ESP-NOW trong điều kiện lắp thật trên xe**: khung/động cơ kim loại có thể che chắn sóng đáng kể. Khoảng cách giữa 2 board trên cùng một xe chỉ vài mét nên tầm xa không phải giới hạn, nhưng **nhiễu/che khuất** thì cần đo thực tế sau khi lắp cố định.
- **ESP-NOW đang chạy không mã hoá** (`peerInfo.encrypt = false`): dữ liệu khoảng cách truyền dạng plaintext. Chấp nhận được ở phạm vi cục bộ trong xe, nhưng nên bật CCMP (AES-128, `lmk`) nếu mở rộng phạm vi hoặc thêm dữ liệu nhạy cảm.
- **Chưa có cơ chế gửi lệnh ngược từ màn hình về `sensor-node`**: nút "Mute Alarm" trên dashboard hiện chỉ tắt cảnh báo **hình ảnh** trên màn hình, không tắt được còi vật lý — muốn làm được cần thêm kênh ESP-NOW 2 chiều (`esp_now_add_peer` ngược lại + xử lý lệnh phía `sensor-node`).
- **Dòng `BUZZER: --` trên sidebar là tàn dư của kiến trúc MQTT, hiện đứng yên vĩnh viễn**: label được tạo trong `build_right_sidebar()` và chỉ được cập nhật bởi `ui_dashboard_set_buzzer_state()` — hàm này **không được gọi từ đâu** trên nhánh ESP-NOW (`src/main.c` chỉ gọi `ui_dashboard_set_relay_state()` một lần lúc khởi tạo). Trước đây Rule-Chain CoreIoT gửi field `buzzer` xuống để màn hình soi trạng thái còi của `sensor-node`; bỏ MQTT thì mất luôn nguồn dữ liệu đó, và ESP-NOW hiện là một chiều nên màn hình **không có cách nào biết** còi đang kêu hay không. Không gây lỗi (label có NULL-guard) nhưng gây hiểu nhầm là đang báo trạng thái thật. Hai hướng xử lý: xoá hẳn dòng đó khỏi sidebar (giữ lại hàm + khai báo trong header cho trường hợp khôi phục MQTT), hoặc bổ sung trạng thái còi vào payload ESP-NOW để hiển thị đúng.
- Tương tự, dòng **`RELAY: -- | N/A (ESP-NOW mode)`** cũng chỉ được set một lần lúc khởi tạo và không bao giờ đổi — `relay`/`warning_status` vốn do Rule-Chain tính, không còn tồn tại trên nhánh này.
- **Cảnh báo cho người đi đường bên ngoài xe** (còi/đèn ngoài xe, xem `architecture.png`) chưa được triển khai — hiện chỉ có cảnh báo trong cabin (còi buzzer + màn hình).
- Tên nhánh `refactor/arduino` không phản ánh đúng bản chất thay đổi (code là ESP-IDF thuần) — cân nhắc đổi tên nhánh, ví dụ `refactor/platformio-layout`.

## 11. Ảnh minh hoạ & đề xuất bổ sung

**Ảnh hiện có trong `report/`:**

| File | Nội dung |
|---|---|
| `architecture.png` | Ý tưởng/đề xuất ban đầu (6 cảm biến, cảnh báo người đi đường) |
| `dashboard-screen-img.jpg` | Ảnh chụp thật dashboard chạy trên Waveshare LCD 7" |
| `rule-chain.png` | Screenshot Rule-Chain CoreIoT |
| `sr04t.png` | Ảnh minh hoạ chung dòng JSN-SR04T, khoanh vùng jumper `R27` chọn Mode (không phải ảnh chụp đúng cấu hình thực tế đang dùng — `sensor-node` chạy JSN-SR04T V3 ở Mode 0 mặc định) |
| `image.png` | Sơ đồ pinout Yolo:Uno (ESP32-S3) |

**Đề xuất bổ sung** (chưa có, nên chụp/vẽ thêm khi có điều kiện):

1. Ảnh toàn cảnh bàn thử nghiệm với cả 2 board (`sensor-node` + `waveshare-screen`) hoạt động đồng thời, thấy rõ dây nối cảm biến.
2. Sơ đồ đấu dây GPIO thực tế của `sensor-node` (breadboard/schematic tay hoặc Fritzing) — hiện chỉ có sơ đồ pinout board trần, chưa có sơ đồ đấu nối cảm biến cụ thể.
3. Ảnh chụp log serial 2 board đặt cạnh nhau (`[ESPNOW] Send OK` phía `sensor-node` và log nhận phía `waveshare-screen`) để minh hoạ luồng dữ liệu đầu-cuối của liên kết ESP-NOW — thay cho ảnh giao diện web CoreIoT vốn không còn nằm trên đường dữ liệu.
