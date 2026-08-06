# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ESP32-S3 firmware for a Waveshare 7" RGB touch LCD (800x480) driving an LVGL "collision-avoidance" dashboard for a truck-mounted array of 6 ultrasonic sensors (JSN-SR04T). This board does not read sensors directly — it receives sensor distances directly over **ESP-NOW** (a local, direct Wi-Fi link, no AP/cloud in between), sent by the sibling project `firmware/sensor-node`, and evaluates hazard locally on-device. `front`/`left_front`/`right_front` currently have real hardware; the other 3 zones render in the UI but get no live data yet (they receive explicit `valid=0` and show as "no data", not stale values).

The project previously used CoreIoT (ThingsBoard) MQTT for this link — that code (`components/coreiot_client/`, Rule-Chain in `cloud/coreiot/rule_chain/`) is still in the tree but **not called on this branch**; it's kept for a possible future MQTT restore. Don't assume MQTT is live when reading old logs/docs that predate the ESP-NOW switch.

Full hardware pinout, sensor layout, ESP-NOW payload schema, and troubleshooting history live in [README.md](README.md) and [`docs/architecture/ESPNOW_NETWORK.md`](../../docs/architecture/ESPNOW_NETWORK.md) — read them for hardware/wiring/protocol details rather than re-deriving them here.

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
- **`coreiot_client`** — *(not used on this branch)* Wi-Fi STA + MQTT wrapper (esp_wifi/esp-mqtt) for CoreIoT, callback-based and deliberately decoupled from UI code. Wi-Fi SSID and MQTT access token live in `components/coreiot_client/include/coreiot_client.h`. Kept in the tree, unreferenced from `src/main.c`.
- **`ui_dashboard`** — the entire LVGL UI (COLLISION tab + SYSTEM tab, sidebars, 2D vehicle canvas, 6 sensor arcs).

ESP-NOW receive logic is not a separate component — it lives directly in `src/main.c` (`on_data_recv()`, registered via `esp_now_register_recv_cb()`), since `esp_now` is a plain ESP-IDF system API with no need for a wrapper.

`src/bsp/waveshare_rgb_lcd_port.{h,c}` is the board support package: RGB LCD panel driver, I2C master bus setup, CH422G IO-expander backlight control, GT911 touch init. GPIO pinout is documented at the top of the header and in the README.

### Threading model

`app_main()` in `src/main.c` initializes the BSP, LVGL adapter, and dashboard on the main task. `WiFi.mode(WIFI_STA)`-equivalent setup pins a fixed channel (`ESPNOW_CHANNEL`, must match `sensor-node`) and registers `esp_now_register_recv_cb(on_data_recv)` — no AP connection, no MQTT task. A 1s `esp_timer` watchdog checks time-since-last-receive and flips the header badge to "NO LINK" after 1.5s of silence.

**LVGL is not thread-safe.** Every `ui_dashboard_*` call must be wrapped in `esp_lv_adapter_lock()/esp_lv_adapter_unlock()`. Two lock timeout conventions matter:
- The main/init task uses `esp_lv_adapter_lock(-1)` (infinite wait) since it owns startup sequencing.
- The ESP-NOW receive callback (`on_data_recv` in `src/main.c`) runs outside the LVGL task, so it uses a bounded `esp_lv_adapter_lock(100)` (100ms) instead — an infinite wait there could deadlock the whole system if the LVGL task ever stalls.

### ESP-NOW data flow

`on_data_recv()` in `src/main.c` receives a fixed-size packed struct `espnow_sensor_msg_t` (6-slot `distance_cm[]`/`valid[]`, **not JSON** — see [`docs/architecture/ESPNOW_NETWORK.md`](../../docs/architecture/ESPNOW_NETWORK.md) for the shared schema both boards must define identically) and, per slot, calls `ui_dashboard_update_sensor()` when `valid=1` or `ui_dashboard_clear_sensor()` when `valid=0`, under the LVGL lock. `evaluate_hazard()` in `ui_dashboard.c` then re-derives the "OVERALL" banner locally, skipping `is_stale` slots. The buzzer is physically driven locally on `sensor-node` and is not reflected on this screen at all on this branch (no MQTT `buzzer` mirror field anymore).

### MQTT data flow (not used on this branch)

`coreiot_client`'s callback-based JSON parsing (accepting keys either at the top level or nested under a `values` object) and `ui_dashboard_set_relay_state()`/`ui_dashboard_set_buzzer_state()` calls still exist in the source but are unreferenced from `src/main.c`. Kept for a possible future MQTT restore — don't assume this path is live.

## Editing components

When changing a component's public API, update both the header in `components/<name>/include/` and any callers in `src/main.c` — there's no build-time interface check across the PlatformIO/ESP-IDF boundary beyond normal compilation. `managed_components/` is PlatformIO/ESP-IDF's dependency cache (LVGL, esp_lvgl_adapter, GT911 touch driver, cJSON, esp-mqtt, etc., resolved via `src/idf_component.yml`) — treat it as vendored, not project code.
