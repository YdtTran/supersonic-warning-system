# LVGL `lv_demos` Architecture & Hardware Audit Reference

> **Reference File**: Audit results of LVGL v9 standard demos and ESP32-S3 Waveshare 800x480 RGB LCD.

---

## 1. Executive Summary & Inventory of `lv_demos`

| Demo Name | Entry Function | Memory Threshold | Primary UI Elements & Features | Relevance to Vehicle Warning Display |
| :--- | :--- | :--- | :--- | :--- |
| **`widgets`** | `lv_demo_widgets()` | Heap: ≥ 38 KB (48 KB rec.) | Multi-tab container (`lv_tabview`), responsive layout engine (`DISP_LARGE`), custom scales/charts, subject/observer dynamic color palette switching. | Provides modular layout architecture (`Profile`, `Analytics`, `Settings`), real-time line charts for distance history, and dynamic warning level color switching. |
| **`music`** | `lv_demo_music()` | Heap: ≥ 64 KB + PSRAM assets | Custom draw callbacks (`LV_EVENT_DRAW_MAIN_BEGIN`), dynamic 20-bar spectrum visualizer (`sin`/`cos` math), animation timelines (`lv_anim_t`), swipe gestures. | Offers architectural blueprint for audio-visual warning effects, animated alert spectrums, and low-latency custom object drawing. |
| **`benchmark`** | `lv_demo_benchmark()` | Heap: ≥ 128 KB | Performance measurement harness (FPS, render time, CPU load via `lv_observer`), multi-scene stress tests (rectangles, text, shadows, image blends, arcs). | Establishes rendering performance baselines and PSRAM bandwidth throughput limits for 800x480 RGB LCD at 16-bit color depth (RGB565). |
| **`stress`** | `lv_demo_stress()` | Heap: ≥ 32 KB | Automated object creation & destruction timer (`obj_test_task_cb`), heap fragmentation tracking (`mem_free_start`), auto-deleting popups/toasts. | Supplies lifecycle patterns for temporary alert popups, toast notifications, memory leak validation (<100B fragmentation limit), and 24/7 display stability. |

---

## 2. Technical Insights from LVGL v9 Demos

### 2.1 Subject/Observer Dynamic Binding Pattern
In LVGL v9, reactive programming replaces explicit widget traversal:
```c
/* Subject definition */
static lv_subject_t distance_subject;

/* Initialization */
lv_subject_init_int(&distance_subject, 150);

/* Observers update automatically */
lv_label_bind_text(distance_label, &distance_subject, "%d cm");
```

### 2.2 Custom Drawing with Integer Trigonometry
High-speed custom drawing callbacks avoid floating-point math overhead:
```c
/* Custom draw callback on LV_EVENT_DRAW_MAIN_BEGIN */
static void spectrum_draw_event_cb(lv_event_t * e) {
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    /* Calculate polar coordinates using precomputed integer sin/cos lookup */
    int32_t x = get_cos(deg, radius);
    int32_t y = get_sin(deg, radius);
    /* Direct framebuffer line draw */
}
```

### 2.3 Memory & PSRAM Allocation Thresholds
- **Internal SRAM Bounce Buffer**: Direct PSRAM RGB LCD refreshes suffer from Wi-Fi DMA bus contention. Allocating a 40-line internal SRAM bounce buffer (`bounce_buffer_size_px = 800 * 40`) eliminates PSRAM bandwidth starvation and screen tearing.
- **PSRAM Framebuffers**: Dual 800x480 RGB565 framebuffers require ~1.5 MB in PSRAM (`.fb_in_psram = 1`).
- **Heap Allocation**: Keep permanent memory fragmentation under 100 bytes during dynamic modal window popups.
