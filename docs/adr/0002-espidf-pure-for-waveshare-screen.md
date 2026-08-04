# 0002. `waveshare-screen` dùng ESP-IDF thuần thay vì Arduino/hybrid

**Trạng thái**: Accepted
**Ngày**: 2026-08-04 (nhánh `refactor/arduino`, merge vào `main` qua commit `2546d9f`)

## Bối cảnh (Context)

Khi tái cấu trúc `firmware/waveshare-screen` sang layout PlatformIO (xem [ADR 0001](0001-platformio-unified-build-system.md)) trên nhánh `refactor/arduino`, mục tiêu ban đầu là đồng bộ **cả framework lẫn layout** với `sensor-node` (Arduino + FreeRTOS), để dùng chung pattern `WiFi.h` + `PubSubClient` (class `CoreiotClient`) cho lớp mạng ở cả 2 board.

Phương án đầu tiên thử là **hybrid `framework = arduino, espidf`**: giữ nguyên driver màn hình/cảm ứng hiện đại (`esp_lvgl_adapter`, `esp_lcd_touch_gt911` bản mới, API I2C `i2c_master.h`, LVGL 9.1, `esp_lcd_panel_rgb` với `num_fbs`/bounce-buffer), đồng thời viết lại `coreiot_client` bằng Arduino core. Phương án này **thất bại thật sự khi build**: các driver trên đòi hỏi **ESP-IDF ≥ 5.5**, nhưng PlatformIO's `espressif32` platform chỉ cấp **ESP-IDF 4.4.7** cho bất kỳ tổ hợp nào có `framework = arduino` (kể cả hybrid `arduino, espidf`). Component Manager báo lỗi version-solving:

```text
ERROR: Because no versions of espressif/esp_lvgl_adapter match ...
  and espressif/esp_lvgl_adapter (0.5.2) depends on idf (>=5.5), ...
  So, because no versions of idf match >=5.5 and project depends on
  espressif/esp_lvgl_adapter (^0.5.2), version solving failed.
```

Đây là giới hạn thật của bộ công cụ đã cài (không phải lỗi code) — phá vỡ hoàn toàn tiền đề của phương án hybrid.

## Quyết định (Decision)

Dùng `framework = espidf` **thuần** cho `waveshare-screen` (không Arduino core dưới bất kỳ hình thức nào), chỉ đổi layout thư mục sang kiểu PlatformIO (`src/`, `boards/yolo_uno.json`, `platformio.ini`) để khớp cách tổ chức project khác trong repo — không đổi framework runtime. `coreiot_client` giữ nguyên `esp_wifi` + `esp-mqtt` (component **esp-mqtt** chính thức của ESP-IDF) thay vì `WiFi.h`/`PubSubClient`, chạy trong một FreeRTOS task riêng (`networkTask`, core 0, tách khỏi task LVGL) theo đúng tinh thần "task-based, non-blocking" giống `sensor-node` — bản thân `esp-mqtt`/`esp_wifi` vốn đã async (event-driven) nên `coreiot_client_init()` không chặn task gọi nó.

**Lưu ý:** tên nhánh `refactor/arduino` không phản ánh đúng bản chất thay đổi cuối cùng — code vẫn là ESP-IDF thuần, không phải Arduino (xem [`report/README.md` mục 10](../../report/README.md#10-hạn-chế--việc-cần-làm-thêm) — có đề xuất đổi tên nhánh nhưng chưa thực hiện).

## Hệ quả (Consequences)

**Tích cực:**
- Giữ nguyên toàn bộ driver màn hình/cảm ứng hiện đại đã hoạt động ổn định (LVGL 9.1, `esp_lvgl_adapter`, `esp_lcd_touch_gt911`, `i2c_master.h`) — không phải hạ cấp hoặc tìm driver thay thế tương thích ESP-IDF 4.4.7 cũ hơn.
- `sensor-node` không cần đổi gì — vẫn dùng làm template tham chiếu cho pattern task-based, non-blocking network layer.
- Layout thư mục vẫn thống nhất với `sensor-node` (`src/`, `platformio.ini`, `build_and_flash.bat` cùng UX) dù framework runtime khác nhau — xem [ADR 0001](0001-platformio-unified-build-system.md).

**Đánh đổi:**
- Không tái sử dụng được các thư viện Arduino-ESP32 tiện lợi (`WiFi.h`, `PubSubClient`) cho lớp mạng — phải viết lại bằng API ESP-IDF gốc dài hơn (`esp_wifi.h`, `esp_event.h`, `esp_netif.h`, `mqtt_client.h`, `nvs_flash.h`), tăng độ phức tạp code so với `sensor-node`.
- 2 firmware trong cùng repo giờ dùng 2 hệ sinh thái thư viện hoàn toàn khác nhau (Arduino Library Manager vs ESP-IDF Component Manager) — người đóng góp mới cần hiểu cả 2 framework, không chỉ 1.
- PlatformIO's ESP-IDF builder đòi hỏi cứng thư mục `src/` ở gốc project bất kể dùng Arduino hay không, buộc đổi tên `main/` (quy ước gốc ESP-IDF) → `src/` — khác biệt nhỏ so với project ESP-IDF thuần bên ngoài PlatformIO.
- Lần build đầu tốn ~545s (tải toolchain + ESP-IDF 6.0.1 + managed_components, ~264MB+) — chấp nhận được vì chỉ xảy ra 1 lần, cache lại cho các lần sau.

## Tham khảo

- [`report/README.md` mục 4](../../report/README.md#4-công-cụ--framework) và [mục 9.4](../../report/README.md#94-quyết-định-kiến-trúc-arduino-hybrid-thất-bại--esp-idf-thuần) — bối cảnh quyết định đầy đủ trong nhật ký phát triển.
- [`docs/logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md`](../logs/waveshare-screen_ARDUINO_REFACTOR_LOG.md) — log kỹ thuật gốc, log lỗi version-solving đầy đủ, danh sách file đã đổi.
- [`docs/API_GUIDE.md` mục 2](../API_GUIDE.md#2-firmwarewaveshare-screen-esp-idf--component-dashboard) — API của các component ESP-IDF thuần kết quả từ quyết định này.
- Commit [`2546d9f`](https://github.com/YdtTran/supersonic-warning-system/commit/2546d9f) — merge kết quả tái cấu trúc vào `main`.
