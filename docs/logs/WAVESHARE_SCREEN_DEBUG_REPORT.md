# Báo Cáo Debug & Khắc Phục Lỗi System: Waveshare ESP32-S3 Screen

**Ngày báo cáo:** 29/07/2026  
**Thiết bị:** Waveshare ESP32-S3-Touch-LCD-7 (800x480 RGB LCD, GT911 Touch Controller)  
**Cổng kết nối:** COM9  
**Môi trường:** ESP-IDF v6.0.2, LVGL v9, CoreIoT MQTT Cloud (`app.coreiot.io`)  

---

## 1. Tổng Quan Vấn Đề Gặp Phải & Nguyên Nhân

| STT | Phân Loại Vấn Đề | Mô Tả Triệu Chứng | Nguyên Nhân Kỹ Thuật (Root Cause) |
| :--- | :--- | :--- | :--- |
| **1** | **Lỗi Font Icon** | Các biểu tượng Icon trên màn hình bị ô vuông `[]`, vỡ nét hoặc hiển thị sai ký tự. | Trong file `ui_app.c` sử dụng chuỗi UTF-8 Emoji 4-byte (ví dụ `⚙️`, `🟢`, `🔴`, `📶`). Font mặc định `lv_font_montserrat_14`/`16` của LVGL không chứa bản đồ mã hóa glyph của Emoji. |
| **2** | **Lỗi Wi-Fi Connecting** | Màn hình đứng ở trạng thái `📶 Wi-Fi: ACLAB (Connecting...)` không lấy được IP. | Bắt được log UART `Reason code: 15` (`WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT`). Mật khẩu Wi-Fi cấu hình cũ (`aclab2023`) sai hoa/thường so với AP thực tế (`ACLAB2023`). |
| **3** | **Cảnh báo Cấp Phát SRAM** | Xuất hiện log `E (1062) alloc partial draw buffer 80000 bytes failed`. | `esp_lv_adapter` thử cấp phát 80KB SRAM nội cho Partial Draw Buffer Tear-Avoid Mode 4. Do SRAM liên tục còn ~191KB, hàm nảy cảnh báo và tự chuyển sang 2 Framebuffer PSRAM (`profile.use_psram = true`). |

---

## 2. Chi Tiết Các Bước Khắc Phục (Fix Applied)

### Fix 1: Thay thế UTF-8 Emoji bằng LVGL Built-in Symbols (`LV_SYMBOL_*`)
Đã cập nhật toàn bộ các chuỗi tiêu đề và badge trạng thái trong file [`waveshare-screen/main/ui/ui_app.c`](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/main/ui/ui_app.c):

```c
// Mã trước khi sửa (lỗi font ô vuông):
lv_label_set_text(title, "⚙️ Wi-Fi & MQTT Key Settings");
lv_label_set_text(s_lbl_mqtt_status, "🟢 MQTT: CONNECTED at broker.coreiot.io");
lv_label_set_text(s_lbl_wifi, "📶 Wi-Fi: Connecting...");
lv_label_set_text(server_title, "🌐 SERVER CONFIG");
lv_label_set_text(cred_title, "🔑 CREDENTIALS");
lv_label_set_text(lbl_show_key, "👁️ Key");

// Mã sau khi sửa (dùng LV_SYMBOL tích hợp sẵn):
lv_label_set_text(title, LV_SYMBOL_SETTINGS " Wi-Fi & MQTT Key Settings");
lv_label_set_text(s_lbl_mqtt_status, LV_SYMBOL_BULLET " MQTT: CONNECTED at broker.coreiot.io");
lv_label_set_text(s_lbl_wifi, LV_SYMBOL_WIFI " Wi-Fi: Connecting...");
lv_label_set_text(server_title, LV_SYMBOL_DIRECTORY " SERVER CONFIG");
lv_label_set_text(cred_title, LV_SYMBOL_FILE " CREDENTIALS");
lv_label_set_text(lbl_show_key, LV_SYMBOL_EYE_OPEN " Key");
```

### Fix 2: Cập Nhật Mật Khẩu Wi-Fi & Bổ Sung Log Chuẩn Đoán Disconnect
Đã cập nhật mật khẩu chính xác trong [`waveshare-screen/main/app_network.h`](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/main/app_network.h) và bổ sung mã lỗi `reason` chi tiết trong [`waveshare-screen/main/app_network.c`](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/main/app_network.c):

```c
// File app_network.h
#define CONFIG_WIFI_SSID            "ACLAB"
#define CONFIG_WIFI_PASS            "ACLAB2023" // Cập nhật mật khẩu chính xác
```

---

## 3. Log Kiểm Định Thực Tế Sau Khi Fix & Nạp Firmware (Verified Logs)

Sau khi nạp firmware mới lên bo mạch **ESP32-S3 (COM9)**, kết quả bắt log thực tế xác nhận hệ thống đã hoạt động hoàn hảo:

```plain
I (1441) app_network: Wi-Fi STA started, connecting to SSID: ACLAB...
I (1449) wifi:state: init -> auth (0xb0)
I (1462) wifi:state: auth -> assoc (0x0)
I (1513) wifi:state: assoc -> run (0x10)
I (1525) wifi:connected with ACLAB, aid = 3, channel 1, BW20, bssid = 44:48:c1:95:47:60
I (1525) wifi:security: WPA2-PSK, phy: bgn, rssi: -46
...
I (2622) esp_netif_handlers: sta ip: 172.28.182.36, mask: 255.255.255.0, gw: 172.28.182.1
I (2622) app_network: Wi-Fi Connected Successfully! IP Address: 172.28.182.36
I (2677) app_network: MQTT Connected to CoreIoT (mqtt://app.coreiot.io:1883)
I (2679) app_network: Published reboot telemetry message to timeseries (msg_id=47026): {"reboot_event":1,"device_id":"30287b60-8a67-11f1-84a8-c17e50898235","status":"ONLINE","reboot_reason":"POWER_ON_RESET","wifi_ssid":"ACLAB"}
```

---

## 4. Kết Luận Trạng Thái Hệ Thống

1. **Hiển thị Màn Hình (LVGL UI)**: Icon hiển thị sắc nét, chuẩn FontAwesome LVGL, không còn bị lỗi font hay ký tự lạ.
2. **Mạng Wi-Fi**: Kết nối thành công tới AP `ACLAB`, được cấp IP `172.28.182.36`.
3. **CoreIoT MQTT Cloud**: Kết nối thành công tới `app.coreiot.io:1883`, đẩy telemetry dữ liệu khởi động thành công lên đám mây.
