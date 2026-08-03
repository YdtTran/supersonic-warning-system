# Dashboard Library & Original Layout Integration Log

**Date**: 2026-07-30  
**Target Device**: ESP32-S3 (Waveshare 7-inch RGB Touch LCD, 800x480)  
**Component Location**: `waveshare-screen/components/dashboard`

---

## 1. Overview & Layout Preservation

Re-integrated the original 800x480 UI layout (`ui_app_init()`) with:
- Top Header Bar (CoreIoT Monitor title, MQTT status badge, Wi-Fi info).
- Left Panel (Server config, Credentials with Access Token toggle, Rule-Chain Warning badge, Action buttons).
- Right Panel (Telemetry grid table with Temperature, Humidity, Distance, Relay, Sparkline Chart, and Action buttons).

The `dashboard` component library interface remains fully available via:

```c
typedef struct {
    const char **items;
} message_list;

typedef struct {
    union {
        uint8_t message_number;
        uint8_t message_numder;
    };
    message_list messages;
    const char *title;
} dashboard;
```

---

## 2. Key Updates

1. **Original Layout Restored**: `ui_app_init()` builds the full original visual dashboard.
2. **Struct Interface Integration**: Calling `ui_app_render_dashboard(&db)` or `dashboard_render(&db)` updates the original layout telemetry rows directly from the `dashboard` struct.
3. **Embedded Parent Support**: Added `dashboard_init_in_parent(parent, w, h)` to allow embedding the single-line concise scrollable list component inside any custom panel or container when needed.

---

## 3. Verification

- Project compiled cleanly with Ninja/CMake without warnings or errors.
- Flashed and verified on ESP32-S3 via COM9.
