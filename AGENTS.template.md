# ESP-IDF Workspace Guidelines & Architecture (Template)

> **Hướng dẫn cấu hình cho thiết bị/máy mới (Cross-Device Setup):**
> 1. Khi chuyển project sang máy mới, hãy sao chép (copy) file này thành `AGENTS.md` ở thư mục gốc của repository.
> 2. Tạo file cấu hình key local: `copy config/keys.template.json config/keys.json` và điền Device Access Token cá nhân vào `config/keys.json`.
> 3. Điền/Sửa các đường dẫn trong dấu `<...>` bên dưới cho khớp với cấu hình ESP-IDF và cổng COM kết nối phần cứng trên máy đó.
> 4. Cả Agent và Người dùng sẽ đọc các secret keys từ file `config/keys.json` (được `.gitignore` bảo mật, không bao giờ push lên Git).

## Overview
This repository contains a **Vehicle Detection & Warning System** built with **ESP32-S3** microcontrollers, the **JSN-SR04T waterproof ultrasonic sensor**, and **CoreIoT (ThingsBoard) Cloud Rule-Chain**.
1. **`firmware/sensor-node/`**: Vehicle detection sensor application using JSN-SR04T waterproof ultrasonic sensor. Measures vehicle distance and publishes telemetry to CoreIoT server via Wi-Fi MQTT (`v1/devices/me/telemetry`). Default Port: **<SENSOR_NODE_PORT, e.g. COM8>**.
2. **`firmware/waveshare-screen/`**: Display application integrated with LVGL graphics library (`lvgl/lvgl`) on a 7-inch RGB Touch LCD. Receives vehicle presence & warning status processed by CoreIoT Rule-Chain via MQTT and renders UI. Default Port: **<WAVESHARE_SCREEN_PORT, e.g. COM9>**.

## Hardware & Environment Configuration
- **Target Chip**: `esp32s3` (Dual-Core 240MHz, Wi-Fi, BT 5 LE, 8MB Embedded PSRAM)
- **ESP-IDF Version**: `<IDF_VERSION, e.g. v6.0.2>`
- **IDF Path**: `<IDF_PATH, e.g. E:\esp\v6.0.2\esp-idf>`
- **IDF Tools Path**: `<IDF_TOOLS_PATH, e.g. C:\Espressif\tools>`
- **Python Virtual Environment**: `<PYTHON_VENV_PATH, e.g. C:\Espressif\tools\python\v6.0.2\venv>`
- **MQTT Test Conda Environment**: `<MQTT_CONDA_VENV, e.g. D:\miniconda\envs\mqtt-coreiot>`
- **Shell Profile**: `<SHELL_PROFILE_PATH, e.g. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1>`
- **Build Generator**: `CMake` with `Ninja` (managed directly or via `idf.py`)

## Included ESP-IDF Libraries & Components
Both projects are configured in `main/CMakeLists.txt` to include and link **all standard ESP-IDF built-in libraries**:
- **Peripheral Drivers**: `driver`, `esp_driver_ledc`, `esp_driver_gpio`, `esp_driver_gptimer`, `esp_driver_i2c`, `esp_driver_spi`, `esp_driver_uart`, `esp_driver_mcpwm`, `esp_driver_pcnt`, `esp_driver_rmt`, `esp_driver_touch_sens`, `esp_adc`, `esp_lcd`
- **Storage & Filesystems**: `nvs_flash`, `spiffs`, `fatfs`, `vfs`
- **System & RTOS**: `freertos`, `log`, `esp_timer`, `esp_event`, `esp_system`
- **Networking**: `esp_wifi`, `esp_netif`, `esp_http_client`, `esp_http_server`
- **Graphics (firmware/waveshare-screen)**: `lvgl/lvgl` (managed via `idf_component.yml`)

## Build & Flash Commands

### Using Project Batch Scripts
Each project contains a standalone `build_and_flash.bat` script that configures the environment, builds with Ninja/CMake, flashes, and monitors output.

**Sensor Node:**
```cmd
cd firmware/sensor-node
build_and_flash.bat build                           :: Build project
build_and_flash.bat flash <SENSOR_NODE_PORT>        :: Flash to specified port
build_and_flash.bat monitor <SENSOR_NODE_PORT>      :: Open Serial Monitor
build_and_flash.bat all <SENSOR_NODE_PORT>          :: Build, Flash & Monitor
```

**Waveshare Screen:**
```cmd
cd firmware/waveshare-screen
build_and_flash.bat build                           :: Build project (fetches LVGL automatically)
build_and_flash.bat flash <WAVESHARE_SCREEN_PORT>   :: Flash to specified port
build_and_flash.bat monitor <WAVESHARE_SCREEN_PORT> :: Open Serial Monitor
build_and_flash.bat all <WAVESHARE_SCREEN_PORT>     :: Build, Flash & Monitor
```

### Direct `idf.py` Commands (when ESP-IDF env is active)
```powershell
# Activate environment in PowerShell:
& '<SHELL_PROFILE_PATH>'

# Set Target to ESP32-S3 (run once per build directory):
idf.py -p <TARGET_PORT> set-target esp32s3

# Build with Ninja:
idf.py build

# Flash and Monitor:
idf.py -p <TARGET_PORT> flash monitor
```

## ESP-IDF Development Best Practices
1. **Component Dependencies**: Use `main/idf_component.yml` for managed components such as `lvgl/lvgl`.
2. **Peripheral Drivers**:
   - `driver/ledc.h` for hardware PWM control.
   - `driver/gpio.h`, `driver/spi_master.h`, `driver/i2c_master.h` for peripherals.
3. **PSRAM Support**: Ensure `CONFIG_SPIRAM=y` is maintained in `sdkconfig.defaults` when allocating framebuffers or large buffers.
4. **Header Cleanliness**: Keep driver inclusions modular in component headers.

## Git Collaboration Workflow
Khi làm việc với dự án, luôn tuân thủ quy trình Git theo 4 bước tiêu chuẩn (**Pull -> Edit -> Commit -> Push**):

1. **Pull (Cập nhật code mới nhất từ remote):**
   ```bash
   git pull origin main --rebase
   ```
   - Luôn kéo mã nguồn mới nhất về trước khi bắt đầu chỉnh sửa để hạn chế xung đột (conflict).

2. **Edit (Thực hiện chỉnh sửa & kiểm định):**
   - Thực hiện thay đổi file/tính năng cần thiết.
   - Kiểm tra build thành công (ví dụ bằng `build_and_flash.bat build`) trước khi chuyển sang bước commit.

3. **Commit (Lưu thay đổi tại local):**
   ```bash
   git add .
   git commit -m "<type>: <mô tả ngắn gọn thay đổi>"
   ```
   - Đặt thông điệp commit rõ ràng (ví dụ: `feat:`, `fix:`, `docs:`, `refactor:`).

4. **Push (Đẩy thay đổi lên remote repository):**
   ```bash
   git push origin main
   ```
   - Nếu bị reject do có commit mới trên remote, chạy `git pull --rebase origin main`, xử lý conflict (nếu có) rồi mới push lại.

## MQTT Testing & CoreIoT Integration
Quy trình kiểm thử kết nối MQTT tới đám mây **CoreIoT (ThingsBoard)** sử dụng môi trường Conda `mqtt-coreiot`:

- **File cấu hình Key local (Gitignored)**: `config/keys.json`
- **File template mẫu trên Git**: `config/keys.template.json`
- **Python Conda Executable**: `<MQTT_PYTHON_PATH, e.g. D:\miniconda\envs\mqtt-coreiot\python.exe>`
- **Broker Host**: `app.coreiot.io` (Port `1883`)
- **Device Access Keys (Tokens)**:
  - **`SENSOR_NODE_DEVICE_TOKEN`**: `<SENSOR_NODE_ACCESS_TOKEN>` *(Sử dụng cho `firmware/sensor-node` gửi telemetry cảm biến JSN-SR04T)*
  - **`WAVESHARE_SCREEN_DEVICE_TOKEN`**: `<WAVESHARE_SCREEN_ACCESS_TOKEN>` *(Sử dụng cho `firmware/waveshare-screen` lấy dữ liệu qua CoreIoT Rule-Chain hiển thị lên màn hình)*
- **Telemetry Topic**: `v1/devices/me/telemetry`
- **Script Test**: `tools/test_mqtt_coreiot.py`

> **Quy tắc bảo mật & Quản lý Key cho AI Agent:**
> - Agent **tuyệt đối không** hardcode secret key vào mã nguồn Python/C++ hoặc file markdown tracked trên Git.
> - Agent sẽ tự động đọc key từ `config/keys.json` local khi chạy script test hoặc cấu hình kết nối.
> - Khi chuyển sang máy mới: Chạy `copy config/keys.template.json config/keys.json` và điền Access Key tương ứng vào `config/keys.json`.

### Các lệnh kiểm thử MQTT:
```powershell
# Gửi 1 gói tin dữ liệu cảm biến mô phỏng tới CoreIoT (Tự động đọc key từ config/keys.json local):
& '<MQTT_PYTHON_PATH>' tools/test_mqtt_coreiot.py

# Gửi dữ liệu liên tục theo chu kỳ 2 giây (Loop mode):
& '<MQTT_PYTHON_PATH>' tools/test_mqtt_coreiot.py --loop --interval 2

# Gửi khoảng cách cố định (ví dụ 15.5 cm):
& '<MQTT_PYTHON_PATH>' tools/test_mqtt_coreiot.py --distance 15.5
```

## Implementation Logging Requirement
Quy tắc bắt buộc sau khi hoàn thành triển khai (implement/fix/refactor):
- Mỗi khi thực hiện xong một nhiệm vụ/tính năng/sửa lỗi, Agent bắt buộc phải tạo hoặc cập nhật một file log Markdown với tên phù hợp mô tả công việc (ví dụ: `docs/logs/<COMPONENT>_<TASK_NAME>_LOG.md`).
- Nội dung file log phải ghi rõ: mục tiêu công việc, các file đã chỉnh sửa, kết quả kiểm thử (build/flash/monitor logs), và hướng dẫn vận hành/chạy demo.



