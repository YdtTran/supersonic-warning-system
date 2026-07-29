# CoreIoT Laptop MQTT Demo & Rule Chain Implementation Log

**Ngày thực hiện**: 2026-07-29  
**Trạng thái**: Completed & Flashed (COM9)  
**Mục tiêu**: Giả lập `sensor-node` gửi telemetry qua MQTT từ laptop lên CoreIoT Server (`app.coreiot.io`), viết CoreIoT Rule Chain định tuyến dữ liệu sang `waveshare-screen` (`30287b60-8a67-11f1-84a8-c17e50898235`) để hiển thị trên màn hình LCD 7 inch.

---

## 📝 Danh sách File đã Chỉnh sửa & Tạo mới

1. **[AGENTS.md](file:///e:/supersonic-sensor-ACLAB/AGENTS.md)** & **[AGENTS.template.md](file:///e:/supersonic-sensor-ACLAB/AGENTS.template.md)**:
   - Thêm quy tắc bắt buộc: Sau khi hoàn thành implement bất kỳ công việc nào, Agent phải tạo/cập nhật file nhật ký markdown (`*_LOG.md`).
2. **[coreiot/rule_chain/supersonic_rule_chain.json](file:///e:/supersonic-sensor-ACLAB/coreiot/rule_chain/supersonic_rule_chain.json)**:
   - Cấu hình lại Rule Chain chuẩn ThingsBoard CoreIoT: tiếp nhận telemetry từ `sensor-node`, xử lý trường khoảng cách (`distance_cm`), nhiệt độ, độ ẩm, phát hiện xe (`vehicle_detected`), rơ-le (`relay`), đổi originator sang `waveshare-screen` và đẩy thuộc tính `SHARED_SCOPE` (`notifyDevice: true`).
3. **[tools/test_mqtt_coreiot.py](file:///e:/supersonic-sensor-ACLAB/tools/test_mqtt_coreiot.py)**:
   - Cập nhật payload JSON khớp với định dạng hiển thị của `waveshare-screen` (`distance_cm`, `distance`, `temperature`, `humidity`, `vehicle_detected`, `relay`, `warning_status`).
4. **[waveshare-screen/main/app_network.c](file:///e:/supersonic-sensor-ACLAB/waveshare-screen/main/app_network.c)**:
   - Thêm đăng ký topic `v1/devices/me/attributes` để đón trực tiếp bản tin đẩy từ Shared Attribute node trên CoreIoT.

---

## 🧪 Kết quả Kiểm thử & Nạp Firmware

### 1. Kiểm thử MQTT Simulator từ Laptop
```text
============================================================
      CoreIoT (ThingsBoard) MQTT Test Publisher
============================================================
Broker Host:  app.coreiot.io:1883
Access Token: OpX...2Vg
Topic:        v1/devices/me/telemetry
============================================================
[MQTT] Kết nối thành công đến app.coreiot.io:1883!
[SEND] Topic: v1/devices/me/telemetry | Payload: {"distance_cm": 15.5, "distance": 15.5, "temperature": 28.5, "humidity": 62.0, "vehicle_detected": true, "relay": "ON", "warning_status": "DANGER", "timestamp": 1785314282510}
[MQTT] Đã phát gói tin thành công (mid=1).
[SUCCESS] Đã gửi thành công 1 gói dữ liệu đến CoreIoT!
```

### 2. Kết quả Build & Flash Firmware Waveshare Screen
```text
esptool --chip esp32s3 -p COM9 -b 460800 write-flash ...
Writing 'waveshare-screen.bin' at 0x00010000...
Wrote 1329552 bytes (839189 compressed) at 0x00010000 in 19.9 seconds (533.4 kbit/s).
Verifying written data... Hash of data verified.
Hard resetting via RTS pin...
Done
```

---

## 🚀 Hướng dẫn Chạy Demo

1. **Giả lập dữ liệu từ Laptop**:
   ```powershell
   & 'D:\miniconda\envs\mqtt-coreiot\python.exe' tools/test_mqtt_coreiot.py --loop --interval 2
   ```

2. **Theo dõi Log Serial từ Màn hình Waveshare (COM9)**:
   ```cmd
   cd waveshare-screen
   build_and_flash.bat monitor COM9
   ```
