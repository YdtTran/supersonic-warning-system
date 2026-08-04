# waveshare-screen: Refactor to PlatformIO / ESP-IDF (yolo_uno)

## Mục tiêu

Chuyển `firmware/waveshare-screen` (trước đây là project ESP-IDF thuần, build bằng `idf.py`/CMake) sang PlatformIO, đồng bộ `board = yolo_uno` với `firmware/sensor-node` (Arduino + FreeRTOS), trên nhánh `refactor/arduino`.

## Quyết định thiết kế (đã đổi hướng giữa chừng — xem lý do)

**Thử đầu tiên: hybrid `framework = arduino, espidf`.** Ý tưởng: giữ driver ESP-IDF (`esp_lcd_panel_rgb`, `esp_lcd_touch_gt911`, `esp_lvgl_adapter`) không đổi, đồng thời viết `coreiot_client` bằng `WiFi.h` + `PubSubClient` (C++ class `CoreiotClient`) giống hệt pattern `firmware/sensor-node/include/CoreiotClient.h`.

**Bị chặn khi build thật:** PlatformIO `espressif32` platform đang cài chỉ cung cấp **ESP-IDF 4.4.7** cho tổ hợp `framework = arduino` / `arduino, espidf`. Nhưng toàn bộ driver màn hình hiện tại (`esp_lvgl_adapter`, `esp_lcd_touch_gt911` bản mới, `i2c_master.h`, LVGL 9.1, `esp_lcd_panel_rgb` với field `num_fbs`/bounce-buffer) đòi hỏi **ESP-IDF >= 5.5**. `pio run -e yolo_uno` thất bại ở bước version-solving của component manager:

```text
ERROR: Because no versions of espressif/esp_lvgl_adapter match ...
  and espressif/esp_lvgl_adapter (0.5.2) depends on idf (>=5.5), ...
  So, because no versions of idf match >=5.5 and project depends on
  espressif/esp_lvgl_adapter (^0.5.2), version solving failed.
```

Đây là giới hạn thật của bộ công cụ đã cài (không phải lỗi code), phá vỡ tiền đề của phương án hybrid.

**Quyết định cuối (được xác nhận lại với người dùng):** dùng `framework = espidf` thuần cho `firmware/waveshare-screen` — vẫn PlatformIO-hóa (`platformio.ini`, `board = yolo_uno`), nhưng **không** dùng Arduino core. Giữ nguyên toàn bộ driver LCD/touch/LVGL (IDF 5.x/6.x). `coreiot_client` giữ `esp_wifi` + esp-mqtt (không phải `WiFi.h`/`PubSubClient` — không có Arduino core để dùng), nhưng chạy trong một FreeRTOS task riêng (`networkTask`, tách khỏi task LVGL) theo đúng tinh thần "task-based, non-blocking" giống sensor-node — bản thân esp-mqtt/esp_wifi vốn đã async (event-driven), `coreiot_client_init()` không block.

`firmware/sensor-node` **không đổi** — đã đạt chuẩn Arduino + FreeRTOS + PlatformIO/yolo_uno từ trước, dùng làm template tham chiếu.

## Phát hiện kỹ thuật quan trọng khác

PlatformIO's ESP-IDF builder (`espidf.py`) **luôn** đòi hỏi một thư mục `src/` tồn tại tại gốc project (biến `PROJECT_SRC_DIR`, mặc định `<project>/src`) — bất kể có dùng Arduino hay không, và bất kể đã có cấu trúc `main/` kiểu ESP-IDF chuẩn hay chưa. Nếu thiếu, build lỗi ngay: `Error: Missing the 'src' folder with project sources.` Vì vậy `main/` đã được đổi tên thành `src/` (giữ nguyên toàn bộ nội dung bên trong: `main.c`, `bsp/`, `CMakeLists.txt`, `idf_component.yml`) để khớp đúng layout PlatformIO ESP-IDF mong đợi — xem ví dụ chính thức `espidf-arduino-blink` đi kèm platform, cũng dùng `src/` chứ không phải `main/`.

## File đã thay đổi

- `firmware/waveshare-screen/platformio.ini` (mới) — `env:yolo_uno`, `framework = espidf`, `board_build.partitions = partitions.csv`.
- `firmware/waveshare-screen/boards/yolo_uno.json` (mới) — copy từ `firmware/sensor-node/boards/yolo_uno.json`.
- `firmware/waveshare-screen/main/` → `firmware/waveshare-screen/src/` (đổi tên thư mục, nội dung giữ nguyên phần lớn).
- `firmware/waveshare-screen/src/main.c` — thêm `networkTask` (FreeRTOS task riêng, core 0) gọi `coreiot_client_set_callbacks()` + `coreiot_client_init()` thay vì gọi thẳng trong `app_main()`; giữ nguyên toàn bộ logic khởi tạo LCD/LVGL/touch và xử lý JSON telemetry.
- `firmware/waveshare-screen/components/coreiot_client/` — giữ nguyên esp_wifi/esp-mqtt (không đổi so với bản gốc).
- `firmware/waveshare-screen/build_and_flash.bat` — chuyển từ gọi `idf.py` trực tiếp sang gọi `pio run`/`pio device monitor` (env `yolo_uno`), giữ nguyên UX `build/flash/monitor/all/clean`.
- `AGENTS.md` — cập nhật mô tả kiến trúc (sensor-node = Arduino/FreeRTOS, waveshare-screen = ESP-IDF thuần qua PlatformIO, kèm phần giải thích "Why not Arduino"), lệnh build/flash chuyển sang `pio run`.

## Không thay đổi (giữ nguyên logic ESP-IDF)

- `src/bsp/waveshare_rgb_lcd_port.c/.h` — driver RGB panel + GT911 + CH422G I2C bring-up.
- `components/coreiot_client/` — esp_wifi + esp-mqtt, API/behavior giống hệt bản gốc.
- `components/sensor_model/` — struct + FreeRTOS mutex, framework-agnostic.
- `components/ui_dashboard/` — toàn bộ logic LVGL.

## Kết quả kiểm thử

- `firmware/sensor-node`: `pio run -e yolo_uno` — **SUCCESS** (2.37s, RAM 13.8%, Flash 21.0%). Không có thay đổi mã nguồn, chỉ xác nhận vẫn build sạch.
- `firmware/waveshare-screen`: `pio run -e yolo_uno` — **SUCCESS** sau khi chuyển sang `framework = espidf` thuần (544.92s lần build đầu tiên — tải toolchain + ESP-IDF 6.0.1 + `managed_components`/LVGL/freetype/libpng/zlib, ~264MB+; các lần sau sẽ nhanh hơn nhiều nhờ cache). RAM 12.7%, Flash 31.1%.
- **Chưa flash/monitor trên phần cứng thật** (không có thiết bị kết nối trong môi trường thực hiện) — cần người có phần cứng xác nhận trước khi merge.
