# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ESP32-S3 firmware for a Waveshare 7" RGB touch LCD (800x480) driving an LVGL "collision-avoidance" dashboard for a truck-mounted array of 6 ultrasonic sensors (JSN-SR04T). This board does not read sensors directly — it receives sensor distances, relay state, and buzzer state over CoreIoT (ThingsBoard) MQTT, published by the sibling project `firmware/sensor-node`. Only `left_front`/`right_front` currently have real hardware; the other 4 zones render in the UI but get no live data yet.

Full hardware pinout, sensor layout, MQTT payload schema, and troubleshooting history live in [README.md](README.md) — read it for hardware/wiring/protocol details rather than re-deriving them here.

## Build system

This is a PlatformIO project (`platformio.ini`, `env:yolo_uno`) but uses `framework = espidf` (pure ESP-IDF, no Arduino), because the display stack (RGB panel bounce-buffers, `i2c_master.h`, `esp_lcd_touch_gt911`, `esp_lvgl_adapter`, LVGL 9.1) requires ESP-IDF >= 5.5, which PlatformIO's Arduino/hybrid combos here don't provide. Because of this, the project still needs the full ESP-IDF CMake layout (`CMakeLists.txt`, `src/`, `components/*`, `sdkconfig.defaults`) — PlatformIO's ESP-IDF builder requires a `src/` root instead of ESP-IDF's usual `main/`, so what would normally be `main/` is named `src/` here.

### Common commands

```cmd
build_and_flash.bat                  :: build only (env: yolo_uno)
build_and_flash.bat flash COM9       :: flash to device
build_and_flash.bat monitor COM9     :: serial monitor, 115200 baud
build_and_flash.bat all COM9         :: build, flash, and monitor
build_and_flash.bat clean            :: wipe build/
```

Default flash port is `COM9`. Equivalent raw PlatformIO: `pio run -e yolo_uno`, `pio run -e yolo_uno -t upload --upload-port COM9`, etc. There is no test suite in this project.

### Debugging a black/unresponsive screen

Use the `esp32_screen_debug` skill (`.agents/skills/esp32_screen_debug/SKILL.md`) rather than guessing — it has a decision tree for I2C driver conflicts, SCL frequency errors, PSRAM framebuffer failures, and GCC 15 attribute errors, all previously hit and fixed on this exact board. When `idf_monitor` isn't usable (non-TTY shell), capture logs with:

```cmd
C:\Espressif\tools\python\v6.0.2\venv\Scripts\python.exe .agents\skills\esp32_screen_debug\scripts\read_serial.py COM9 5
```

## Architecture

Three custom ESP-IDF components under `components/`, wired together by `src/main.c`:

- **`sensor_model`** — thread-safe (mutex-guarded) struct holding the 6 sensor distances + mount offsets. Pure data model, no I/O.
- **`coreiot_client`** — Wi-Fi STA + MQTT wrapper (esp_wifi/esp-mqtt) for CoreIoT, callback-based and deliberately decoupled from UI code. Wi-Fi SSID and MQTT access token live in `components/coreiot_client/include/coreiot_client.h`.
- **`ui_dashboard`** — the entire LVGL UI (COLLISION tab + SYSTEM tab, sidebars, 2D vehicle canvas, 6 sensor arcs).

`src/bsp/waveshare_rgb_lcd_port.{h,c}` is the board support package: RGB LCD panel driver, I2C master bus setup, CH422G IO-expander backlight control, GT911 touch init. GPIO pinout is documented at the top of the header and in the README.

### Threading model

`app_main()` in `src/main.c` initializes the BSP, LVGL adapter, and dashboard on the main task, then spawns a separate `networkTask` (pinned to core 0) that calls `coreiot_client_init()`. That init call is non-blocking — it registers event handlers and returns; all subsequent Wi-Fi/MQTT connection and message handling happens via `esp_event` and esp-mqtt's own internal task.

**LVGL is not thread-safe.** Every `ui_dashboard_*` call must be wrapped in `esp_lv_adapter_lock()/esp_lv_adapter_unlock()`. Two lock timeout conventions matter:
- The main/init task uses `esp_lv_adapter_lock(-1)` (infinite wait) since it owns startup sequencing.
- Wi-Fi/MQTT callbacks (`on_wifi_status`, `on_mqtt_status`, `on_mqtt_data` in `src/main.c`) run on the Wi-Fi event loop / esp-mqtt task, *not* the LVGL task, so they use a bounded `LV_LOCK_TIMEOUT_TICKS` (100ms) instead — an infinite wait there could deadlock the whole system if the LVGL task ever stalls.

### MQTT data flow

`on_mqtt_data()` in `src/main.c` parses incoming JSON (accepting keys either at the top level or nested under a `values` object — CoreIoT sends both shapes depending on context), matches sensor keys via `k_sensor_json_keys[]` (positionally aligned with `sensor_id_t` in `sensor_model.h`), and calls `ui_dashboard_update_sensor()` / `ui_dashboard_set_relay_state()` / `ui_dashboard_set_buzzer_state()` under the LVGL lock. The buzzer is physically driven locally on `sensor-node` (not round-tripped through the cloud, to keep latency low); the `buzzer` MQTT field here is purely for mirroring that state on-screen.

## Editing components

When changing a component's public API, update both the header in `components/<name>/include/` and any callers in `src/main.c` — there's no build-time interface check across the PlatformIO/ESP-IDF boundary beyond normal compilation. `managed_components/` is PlatformIO/ESP-IDF's dependency cache (LVGL, esp_lvgl_adapter, GT911 touch driver, cJSON, esp-mqtt, etc., resolved via `src/idf_component.yml`) — treat it as vendored, not project code.
