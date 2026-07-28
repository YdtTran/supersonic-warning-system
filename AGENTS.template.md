# ESP-IDF Workspace Guidelines & Architecture (Template)

> **Hướng dẫn cấu hình cho thiết bị/máy mới (Cross-Device Setup):**
> 1. Khi chuyển project sang máy mới, hãy sao chép (copy) file này thành `AGENTS.md` ở thư mục gốc của repository.
> 2. Điền/Sửa các đường dẫn trong dấu `<...>` bên dưới cho khớp với cấu hình ESP-IDF và cổng COM kết nối phần cứng trên máy đó.
> 3. Cả Agent và Người dùng sẽ sử dụng thông tin trong `AGENTS.md` đã tinh chỉnh để thực thi các lệnh build, flash và monitor.

## Overview
This repository contains two independent ESP-IDF projects targeted for **ESP32-S3** microcontrollers:
1. **`sensor-node/`**: Core sensor node application (includes peripherals like LEDC PWM, GPIO, SPI/I2C sensors). Default Port: **<SENSOR_NODE_PORT, e.g. COM8>**.
2. **`waveshare-screen/`**: Display application integrated with LVGL graphics library (`lvgl/lvgl`). Default Port: **<WAVESHARE_SCREEN_PORT, e.g. COM9>**.

## Hardware & Environment Configuration
- **Target Chip**: `esp32s3` (Dual-Core 240MHz, Wi-Fi, BT 5 LE, 8MB Embedded PSRAM)
- **ESP-IDF Version**: `<IDF_VERSION, e.g. v6.0.2>`
- **IDF Path**: `<IDF_PATH, e.g. E:\esp\v6.0.2\esp-idf>`
- **IDF Tools Path**: `<IDF_TOOLS_PATH, e.g. C:\Espressif\tools>`
- **Python Virtual Environment**: `<PYTHON_VENV_PATH, e.g. C:\Espressif\tools\python\v6.0.2\venv>`
- **Shell Profile**: `<SHELL_PROFILE_PATH, e.g. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1>`
- **Build Generator**: `CMake` with `Ninja` (managed directly or via `idf.py`)

## Included ESP-IDF Libraries & Components
Both projects are configured in `main/CMakeLists.txt` to include and link **all standard ESP-IDF built-in libraries**:
- **Peripheral Drivers**: `driver`, `esp_driver_ledc`, `esp_driver_gpio`, `esp_driver_gptimer`, `esp_driver_i2c`, `esp_driver_spi`, `esp_driver_uart`, `esp_driver_mcpwm`, `esp_driver_pcnt`, `esp_driver_rmt`, `esp_driver_touch_sens`, `esp_adc`, `esp_lcd`
- **Storage & Filesystems**: `nvs_flash`, `spiffs`, `fatfs`, `vfs`
- **System & RTOS**: `freertos`, `log`, `esp_timer`, `esp_event`, `esp_system`
- **Networking**: `esp_wifi`, `esp_netif`, `esp_http_client`, `esp_http_server`
- **Graphics (waveshare-screen)**: `lvgl/lvgl` (managed via `idf_component.yml`)

## Build & Flash Commands

### Using Project Batch Scripts
Each project contains a standalone `build_and_flash.bat` script that configures the environment, builds with Ninja/CMake, flashes, and monitors output.

**Sensor Node:**
```cmd
cd sensor-node
build_and_flash.bat build                           :: Build project
build_and_flash.bat flash <SENSOR_NODE_PORT>        :: Flash to specified port
build_and_flash.bat monitor <SENSOR_NODE_PORT>      :: Open Serial Monitor
build_and_flash.bat all <SENSOR_NODE_PORT>          :: Build, Flash & Monitor
```

**Waveshare Screen:**
```cmd
cd waveshare-screen
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
