# CoreIoT Rule-Chain Data Routing & Screen Warning Dashboard Implementation Log

## Overview
Implemented MQTT telemetry data routing from CoreIoT (ThingsBoard) Rule-Chain to the Waveshare 7-inch LCD screen dashboard. Extended JSON payload parsing, created visual warning status badges mapped to HSL color design tokens, reconfigured module build scripts for fast incremental builds, and implemented smart simulation fallback.

## Modified Files
1. **`waveshare-screen/build_and_flash.bat`**:
   - Reconfigured `%ACTION% == build` to execute `idf.py build` for fast Ninja incremental compilation without requiring physical serial ports.
   - Cleaned PowerShell exit status so module script returns standard exit code 0.
2. **`sensor-node/build_and_flash.bat`**:
   - Reconfigured `%ACTION% == build` to execute `idf.py build` for fast Ninja incremental compilation.
   - Cleaned PowerShell exit status.
3. **`waveshare-screen/main/ui/ui_app.h`**:
   - Declared `ui_app_update_warning_status(const char *status, float distance, bool vehicle_detected)` API.
4. **`waveshare-screen/main/ui/ui_app.c`**:
   - Added `s_warning_badge` container and `s_lbl_warning_status` text label on the left panel surface.
   - Implemented `ui_app_update_warning_status` mapping warning states (`SAFE`, `APPROACHING`, `WARNING`, `DANGER`) to HSL color design tokens:
     - `SAFE`/`NORMAL`: Emerald Green (`#10B981`)
     - `APPROACHING`: Amber Yellow (`#F59E0B`)
     - `WARNING`: Vivid Orange (`#F97316`)
     - `DANGER`/`CRITICAL`: Crimson Red (`#EF4444`)
5. **`waveshare-screen/main/app_network.h`**:
   - Added `#include <stdint.h>` header inclusion.
   - Declared `app_network_has_recent_data(uint32_t max_age_ms)` API.
6. **`waveshare-screen/main/app_network.c`**:
   - Extended JSON parser in `mqtt_event_handler` to extract both root keys and `values` wrapper objects from CoreIoT Rule-Chain messages.
   - Tracked last MQTT payload reception timestamp using `esp_timer_get_time()`.
   - Forwarded telemetry and warning status updates to LVGL under `esp_lv_adapter_lock()`.
7. **`waveshare-screen/main/main.c`**:
   - Completely removed synthetic `telemetry_sim_task` generation function and FreeRTOS task creation to ensure CoreIoT MQTT server is the single root of trust.
8. **`waveshare-screen/main/ui/ui_app.c`**:
   - Initialized telemetry table fields with `"----"` and `"Waiting..."` placeholders.
   - Set Warning Status Badge initial state to `LV_SYMBOL_REFRESH " WAITING FOR SERVER\nNo telemetry received"`.
   - Updated `publish_test_cb` to send a server ping/request telemetry payload (`{"ping":true,"client":"waveshare_screen"}`) without generating local synthetic numbers.

## Verification Results

### 1. Incremental Build Verification
- Executed `build_and_flash.bat build` in `waveshare-screen`:
  ```cmd
  cd waveshare-screen
  build_and_flash.bat build
  ```
  **Result:** Success (Exit code: 0, binary size: `0x1457e0` bytes).
- Executed `build_and_flash.bat build` in `sensor-node`:
  ```cmd
  cd sensor-node
  build_and_flash.bat build
  ```
  **Result:** Success (Exit code: 0, binary size: `0x2fed0` bytes).

### 2. CoreIoT MQTT Integration Verification
- Executed `tools/test_mqtt_coreiot.py` with device token `lyeFK1raLOPmjx7bEApw`:
  ```powershell
  & 'D:\miniconda\envs\mqtt-coreiot\python.exe' tools/test_mqtt_coreiot.py --token lyeFK1raLOPmjx7bEApw --distance 18.5
  ```
  **Result:** Successfully published JSON payload `{"distance": 18.5, "warning_status": "DANGER", "vehicle_detected": true, "relay": "ON"}` to `app.coreiot.io:1883`.

### 3. Hardware Flashing & UART Serial Log Debugging
- Executed hardware flashing to COM9:
  ```cmd
  cd waveshare-screen && build_and_flash.bat flash COM9
  ```
  **Result:** Successfully flashed 1.33 MB binary to ESP32-S3 at `0x00010000`. Reset via RTS pin succeeded.
- Captured UART serial boot logs (`.agents/skills/esp32_screen_debug/scripts/read_serial.py COM9 5`):
  ```text
  I (585) bsp_lcd_port: Install RGB LCD panel driver
  I (638) bsp_lcd_port: Initialize RGB LCD panel
  I (1040) bsp_lcd_port: Initialize touch controller GT911
  I (1046) GT911: TouchPad_ID:0x39,0x31,0x31
  I (1158) esp_lvgl:adapter: LVGL task started successfully
  I (2632) app_network: Wi-Fi Connected Successfully! IP Address: 172.28.182.36
  I (2911) app_network: MQTT Connected to CoreIoT (mqtt://app.coreiot.io:1883)
  ```
- Discovered and verified token isolation: `sensor-node` uses `OpXQiVBnETXAgVehd2Vg` while `waveshare-screen` uses `lyeFK1raLOPmjx7bEApw` to allow concurrent MQTT subscriptions without broker disconnection.
- Verified end-to-end telemetry forwarding from `sensor-node` through CoreIoT Rule-Chain to `waveshare-screen`:
  ```text
  I (166741) app_network: MQTT DATA received from topic v1/devices/me/attributes: {"distance":16.5,"distance_cm":16.5,"vehicle_detected":true,"warning_status":"DANGER","relay":"ON","temperature":28.5,"humidity":62,"source_device":"sensor-node"}
  ```

## Operational Guide
- **To perform incremental compilation on screen project:**
  `cd waveshare-screen && build_and_flash.bat build`
- **To flash screen project to target board:**
  `cd waveshare-screen && build_and_flash.bat flash COM9`
- **To monitor serial output:**
  `cd waveshare-screen && build_and_flash.bat monitor COM9`
- **To simulate continuous vehicle telemetry from CoreIoT:**
  `& 'D:\miniconda\envs\mqtt-coreiot\python.exe' tools/test_mqtt_coreiot.py --token OpXQiVBnETXAgVehd2Vg --loop --interval 2`
