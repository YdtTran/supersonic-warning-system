# Hướng dẫn Đóng góp (Contributing)

> Dành cho người (và AI agent) muốn đóng góp code/tài liệu cho dự án. Cấu hình máy/agent-specific (đường dẫn ESP-IDF, cổng COM, secrets) nằm ở [`AGENTS.md`](AGENTS.md) (local, không commit) / [`AGENTS.template.md`](AGENTS.template.md) (template) — file này chỉ nói về **quy trình làm việc chung**.

## 1. Bắt đầu

1. Clone repo kèm submodule, tạo file key local — xem [README.md mục 2](README.md#-2-hướng-dẫn-cài-đặt--cấu-hình-nhanh-quick-start).
2. Nếu chuyển sang máy mới, copy `AGENTS.template.md` → `AGENTS.md` và điền cấu hình máy — xem [README.md mục Bước 3](README.md#bước-3-tự-động-quét-môi-trường-cross-device-setup).
3. Đọc [`docs/README.md`](docs/README.md) để biết tài liệu nào đọc trước khi code.

## 2. Quy trình Git (Pull → Edit → Commit → Push)

Dự án hiện đẩy trực tiếp lên `main` (quy mô nhóm nhỏ, không dùng branch/PR bắt buộc). Luôn tuân thủ 4 bước sau để hạn chế conflict:

### Bước 1 — Pull

```bash
git pull origin main --rebase
```

Luôn kéo mã nguồn mới nhất về trước khi bắt đầu chỉnh sửa.

### Bước 2 — Edit & kiểm định

- Thực hiện thay đổi cần thiết.
- **Bắt buộc build thành công trước khi commit**:

  ```bash
  cd firmware/sensor-node && build_and_flash.bat build
  cd firmware/waveshare-screen && build_and_flash.bat build
  ```

  Chỉ cần build lại firmware bị ảnh hưởng bởi thay đổi của bạn.

### Bước 3 — Commit

```bash
git add <file cụ thể>   # tránh git add -A/. để không lỡ commit secrets/build artifacts
git commit -m "<type>: <mô tả ngắn gọn>"
```

Quy ước prefix commit message (theo lịch sử repo hiện tại):

| Prefix | Khi dùng |
| --- | --- |
| `feat:` | Thêm tính năng mới |
| `fix:` | Sửa lỗi |
| `docs:` | Chỉ thay đổi tài liệu (`.md`, comment) |
| `refactor:` | Tái cấu trúc code, không đổi hành vi |
| `chore:` | Việc lặt vặt (dọn file, cấu hình build, dependency) |

### Bước 4 — Push

```bash
git push origin main
```

Nếu bị reject do có commit mới trên remote: `git pull --rebase origin main`, xử lý conflict rồi push lại. Không dùng `git push --force` lên `main` trừ khi đã thống nhất với người còn lại trong nhóm.

## 3. Quy ước code

- **`firmware/sensor-node`** (`framework = arduino`): không dùng `std::vector`/cấp phát động trong đường đo tốc độ cao (tránh phân mảnh heap) — mỗi cảm biến 1 instance `UltrasonicSensor`/`DistanceFilter` riêng, không share state qua ISR. Chi tiết API: [`docs/API_GUIDE.md` mục 1](docs/API_GUIDE.md#1-firmwaresensor-node-arduino--thư-viện-đo--lọc-cảm-biến).
- **`firmware/waveshare-screen`** (`framework = espidf` thuần): mọi hàm `ui_dashboard_*` phải gọi trong `esp_lv_adapter_lock()`/`unlock()`; callback chạy trên task khác task LVGL (Wi-Fi/MQTT) **luôn dùng timeout hữu hạn** (vd `lock(100)`), không dùng `-1` — tránh deadlock. Chi tiết: [`docs/API_GUIDE.md` mục 2.3](docs/API_GUIDE.md#23-ui_dashboard--giao-diện-lvgl-91).
- Component/library mới nên giữ trách nhiệm đơn lẻ (single responsibility) theo mẫu hiện có (`sensor_model`/`coreiot_client`/`ui_dashboard` độc lập nhau).
- **Không hardcode secret mới** (token, password) vào code — đọc từ `config/keys.json` (gitignored) khi có thể; nếu bắt buộc hardcode trong header firmware (hạn chế hiện tại, xem [`SECURITY.md`](SECURITY.md)), không thêm token/password thật mới vào file đã tracked mà không cân nhắc.

## 4. Ghi log triển khai (Implementation Logging)

**Bắt buộc** sau khi hoàn thành một nhiệm vụ/tính năng/sửa lỗi: tạo hoặc cập nhật 1 file log Markdown tại `docs/logs/<COMPONENT_hoặc_CHỦ_ĐỀ>_<MÔ_TẢ>_LOG.md`.

Nội dung log cần có:
- Mục tiêu công việc.
- Danh sách file đã chỉnh sửa.
- Kết quả kiểm thử (build/flash/monitor log, hoặc lý do chưa test được trên phần cứng thật).
- Hướng dẫn vận hành/chạy demo (nếu có).

Đây là nhật ký kỹ thuật chi tiết (dev diary) — khác với `report/README.md` mục 9 (tóm tắt lịch sử phát triển cho báo cáo) và (đề xuất) `CHANGELOG.md` (tóm tắt theo version cho người dùng cuối). Xem ví dụ log hiện có tại [`docs/logs/`](docs/logs/).

## 5. Kiểm thử MQTT trước khi thay đổi Rule-Chain/firmware networking

Trước khi đổi logic MQTT (`CoreiotClient`, `coreiot_client`, Rule-Chain), dùng script mô phỏng để kiểm tra không cần phần cứng thật:

```powershell
& 'D:\miniconda\envs\mqtt-coreiot\python.exe' tools/test_mqtt_coreiot.py --loop --interval 2
```

Chi tiết: [`docs/API_GUIDE.md` mục 4](docs/API_GUIDE.md#4-rule-chain-coreiot--cấu-hình--api-node).
