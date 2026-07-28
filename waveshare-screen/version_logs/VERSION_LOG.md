# Waveshare Screen - Version Log & Troubleshooting History

Target Module: `waveshare-screen/`  
Microcontroller: ESP32-S3 (Dual-Core 240MHz, 8MB Octal PSRAM, 8MB Flash)  
Display Hardware: Waveshare ESP32-S3 Touch LCD 7 (800x480 RGB LCD, CH422G I2C Backlight, GT911 Touch)  
Software Environment: ESP-IDF `v6.0.2`, GCC `15.2.0`, LVGL `v9.5.0` / `v8` adapter  

---

## Version Releases

### [v1.2.0] - 2026-07-28
#### Added
- **UI Application**: Created `"ACLAB 2023"` animated label (Montserrat 36px font, Sky Blue `#38BDF8`) continuously moving top-to-bottom ($Y = -60$ to $Y = 480$) in a 3.5-second infinite loop.
- **Modularized Code Structure**: Split codebase into `main/bsp/` (hardware LCD & Touch driver) and `main/ui/` (UI canvas & animation engine).
- **Debug Skill (`esp32_screen_debug`)**: Created specialized agent skill in `.agents/skills/esp32_screen_debug` with non-blocking Python UART capture tool `scripts/read_serial.py`.

---

### [v1.1.0] - 2026-07-28
#### Added
- **Incremental Build Optimization**: Updated `build_and_flash.bat` to preserve `build/` directory across builds, reducing re-compile times from minutes (1961 files) down to 2 seconds.
- **Auto-Flash Integration**: Configured `build_and_flash.bat` without arguments to automatically flash firmware directly to COM9 after incremental build.

---

### [v1.0.0] - 2026-07-28
#### Added
- Initial porting of Waveshare RGB LCD driver and CH422G backlight controller to ESP-IDF v6.0.2.

---

## Detailed History of Issues & Solutions (Các vấn đề đã gặp & Xử lý)

### 1. `ISSUE-01`: GCC 15 Attribute Directives Error (`-Werror=attributes`)
- **Triệu chứng**: Quá trình biên dịch thất bại với lỗi `error: 'attributes' attribute directive ignored [-Werror=attributes]` tại các macro memory của LVGL (`IRAM_ATTR` / `LV_ATTRIBUTE_FAST_MEM`).
- **Nguyên nhân**: Trình biên dịch GCC 15 nghiêm ngặt hơn đối với vị trí attribute trên biến và hàm.
- **Giải pháp**: Bổ sung cờ bỏ qua cảnh báo attribute trong file [CMakeLists.txt](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/CMakeLists.txt) gốc:
  ```cmake
  add_compile_options("-Wno-attributes")
  ```

---

### 2. `ISSUE-02`: Thay đổi cấu trúc API ESP-IDF v6.0.2 (`esp_lcd_rgb_panel_config_t`)
- **Triệu chứng**: Lỗi biên dịch `has no member named 'bits_per_pixel'`, `sram_trans_align`, `psram_trans_align` khi mang code driver từ ESP-IDF v5 sang ESP-IDF v6.
- **Nguyên nhân**: ESP-IDF v6.0.2 cơ cấu lại struct cấu hình màn hình RGB (`esp_lcd_panel_rgb.h`), thay thế `bits_per_pixel` bằng định dạng màu `in_color_format` / `out_color_format`.
- **Giải pháp**: Cập nhật struct initializer trong [waveshare_rgb_lcd_port.c](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/main/bsp/waveshare_rgb_lcd_port.c) tương thích 100% với ESP-IDF v6.0.2 trong khi bảo toàn nguyên vẹn sơ đồ chân hardware (HSYNC=46, VSYNC=3, DE=5, PCLK=7, Data 0-15).

---

### 3. `ISSUE-03`: Trực tiếp kích hoạt `fullclean` mỗi lần chạy Build (`0/1961` rebuild)
- **Triệu chứng**: Mỗi lần gõ lệnh `build_and_flash.bat build`, hệ thống lại tự xóa thư mục `build/` và biên dịch lại từ đầu toàn bộ 1961 file.
- **Nguyên nhân**: Script cũ luôn gọi `idf.py set-target esp32s3`. Trong ESP-IDF, lệnh `set-target` tự động chèn hành động `fullclean` xóa sạch thư mục build.
- **Giải pháp**: Cập nhật [build_and_flash.bat](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/build_and_flash.bat) kiểm tra thư mục `build/`. Lệnh `set-target` chỉ được gọi một lần duy nhất khi dự án chưa được tạo build dir.

---

### 4. `ISSUE-04`: Xung đột I2C Kernel Panic (`CONFLICT! driver_ng`)
- **Triệu chứng**: Màn hình bị tối đen hoàn toàn. Vi điều khiển bị reset liên tục ngay khi cắm nguồn. Bắt log UART qua cổng COM9 phát hiện lỗi:
  ```text
  E (439) i2c: CONFLICT! driver_ng is not allowed to be used with this old driver
  abort() was called at PC 0x420427ef on core 0
  ```
- **Nguyên nhân**: Component cảm ứng GT911 (`esp_lcd_touch_gt911` v1.2.0) sử dụng driver I2C mới của ESP-IDF v6 (`driver/i2c_master.h` / `driver_ng`), trong khi file porting màn hình cũ lại khởi tạo I2C legacy (`i2c_driver_install`). Việc dùng chung 2 driver này gây kernel panic.
- **Giải pháp**: Tái cấu trúc file [waveshare_rgb_lcd_port.c](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/main/bsp/waveshare_rgb_lcd_port.c) chuyển toàn bộ điều khiển IC CH422G và GT911 sang driver I2C mới (`i2c_new_master_bus`, `i2c_master_bus_add_device`, `i2c_master_transmit`).

---

### 5. `ISSUE-05`: Tần số SCL I2C Cảm ứng không hợp lệ (`invalid scl frequency`)
- **Triệu chứng**: Khởi tạo Panel IO thất bại với lỗi `ESP_ERR_INVALID_ARG` tại hàm `esp_lcd_new_panel_io_i2c`. Log UART hiển thị:
  ```text
  E (918) i2c.master: i2c_master_bus_add_device(1176): invalid scl frequency
  ```
- **Nguyên nhân**: Biến `tp_io_config.scl_speed_hz` được thiết lập bằng `0`. Trong ESP-IDF v6, driver I2C mới yêu cầu `scl_speed_hz` phải là một số dương cụ thể.
- **Giải pháp**: Đặt cờ `tp_io_config.scl_speed_hz = 400 * 1000` (400kHz Fast-mode I2C) cho bảng cảm ứng GT911.

---

### 6. `ISSUE-06`: Thiếu Kconfig Symbol cho LVGL Widgets Demo
- **Triệu chứng**: Trình biên dịch không tìm thấy hàm `lv_demo_widgets()` khi thử nghiệm ví dụ `08_lvgl_v8_demo`.
- **Nguyên nhân**: Thiếu cấu hình cờ biên dịch demo trong Kconfig.
- **Giải pháp**: Thêm `CONFIG_LV_USE_DEMO_WIDGETS=y` vào [sdkconfig.defaults](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/sdkconfig.defaults) và [sdkconfig](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/sdkconfig).
