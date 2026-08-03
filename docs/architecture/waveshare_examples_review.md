# Waveshare ESP-IDF Examples & LVGL Architecture Review Report

> **Document Version**: 1.0 (Session 1 Complete)  
> **Target Hardware**: Waveshare ESP32-S3 7-inch RGB Touch LCD  
> **SDK**: ESP-IDF v5.x / v6.x

---

## 1. Overview & Inventory of Manufacturer Examples

The manufacturer examples located in [`reference/lcd-example/examples/ESP-IDF`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF) showcase peripheral integration for the Waveshare ESP32-S3 development platform.

| Module | Location | Primary Peripherals / Drivers | Practical Function / Purpose |
| :--- | :--- | :--- | :--- |
| **01_I2C_Test** | [`01_I2C_Test/`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/01_I2C_Test) | `driver/i2c_master.h`, `esp_console` | Interactive I2C bus scanner & register read/write REPL console (`i2cdetect`, `i2cget`, `i2cset`). |
| **02_RS485_Test** | [`02_RS485_Test/`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/02_RS485_Test) | `driver/uart.h` | Half-duplex RS485 UART echo receiver & transmitter with IRAM interrupt capability. |
| **03_SD_Test** | [`03_SD_Test/`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/03_SD_Test) | `esp_vfs_fat_sdspi`, `driver/spi_master.h`, CH422G | SD Card mounting over SPI, FatFS file read/write/rename, CH422G IO expander CS pin control. |
| **04_Sensor_AD** | [`04_Sensor_AD/`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/04_Sensor_AD) | `esp_adc/adc_oneshot.h`, `esp_adc/adc_cali.h` | Oneshot ADC raw reading & Curve-Fitting calibration to convert raw samples to millivolts. |
| **05_UART_Test** | [`05_UART_Test/`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/05_UART_Test) | `driver/uart.h` | Basic serial data echo loop. |
| **06_TWAItransmit** | [`06_TWAItransmit/`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/06_TWAItransmit) | `driver/twai.h`, CH422G, FSUSB42UMX | CAN Bus (TWAI) message transmission, alert monitoring, and FSUSB42UMX pin multiplexer switching. |
| **07_TWAIreceive** | [`07_TWAIreceive/`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/07_TWAIreceive) | `driver/twai.h`, CH422G, FSUSB42UMX | CAN Bus (TWAI) message reception queue and alert logging. |
| **08_lvgl_v8_demo** | [`08_lvgl_v8_demo/`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/08_lvgl_v8_demo) | `esp_lcd_rgb_panel`, GT911, LVGL v8, PSRAM | LVGL v8 RGB 800x480 GUI demo with GT911 touch reset sequence via CH422G and PSRAM bounce buffering. |
| **09_lvgl_v9_demo** | [`09_lvgl_v9_demo/`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/09_lvgl_v9_demo) | `esp_lcd_rgb_panel`, GT911, LVGL v9, PSRAM | LVGL v9 GUI porting layer with updated display/indev registration APIs. |

---

## 2. Hardware Driver Architecture & Real-World Handling

### 2.1 CH422G I2C IO Expander Architecture
A core design pattern on Waveshare ESP32-S3 boards is the inclusion of a **CH422G I2C IO Expander** mapped at I2C write addresses `0x24` and `0x38`. 

The CH422G controls critical hardware control lines:
- **LCD Backlight**: Powering on backlight requires writing `0x1E` to register `0x38`.
- **GT911 Touch Controller Reset**: Toggling touch hardware reset requires sending pulse sequences (`0x2C` followed by `0x2E` to register `0x38`) combined with GPIO control.
- **SD Card Chip Select (CS)**: Toggling SD CS line requires configuring EXIO via CH422G commands (`0x01` to `0x24`, `0x0A` to `0x38`).
- **CAN/TWAI USB MUX Switch**: Waveshare shares GPIO19/20 between USB D+/D- and CAN TX/RX. Toggling `FSUSB42UMX` chip via CH422G switches physical routing from USB to CAN transceiver (`0x20` to `0x38`).

> [!IMPORTANT]
> **Real-World Risk**: Because CH422G, GT911 touch, and other sensors share the same I2C bus (`I2C_NUM_0`), concurrent access from different FreeRTOS tasks (e.g. LVGL input task and Background Control task) **will cause I2C bus arbitration collisions** if not protected by an I2C bus mutex (`lvgl_port_lock` or custom mutex).

---

### 2.2 RGB Display & Memory (PSRAM) Management
Located in [`waveshare_rgb_lcd_port.c`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/08_lvgl_v8_demo/components/waveshare_rgb_lcd_port.c):

1. **PSRAM Framebuffer Allocation (`.fb_in_psram = 1`)**:
   - Resolution 800x480 @ 16-bit (RGB565) requires **768 KB per frame buffer**. Dual buffering requires **~1.5 MB**, which exceeds internal SRAM (~512 KB).
   - The driver allocates framebuffers in PSRAM (`MALLOC_CAP_SPIRAM`).
2. **Bounce Buffer Mechanism (`bounce_buffer_size_px`)**:
   - Direct RGB Panel refresh from PSRAM over SPI/Octal PSRAM bus can suffer from **PSRAM bandwidth starvation** when Wi-Fi or MQTT operations run concurrently, causing screen drift/tearing.
   - Waveshare configures a bounce buffer in internal SRAM (`bounce_buffer_size_px = EXAMPLE_RGB_BOUNCE_BUFFER_SIZE`) which transfers pixel blocks via DMA from PSRAM to internal SRAM before clocking out to the RGB LCD panel.

---

### 2.3 CAN/TWAI Communication & USB MUX Switching
Located in [`waveshare_twai_port.c`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/06_TWAItransmit/main/waveshare_twai_port.c):

1. **Hardware Pin MUXing**:
   - `waveshare_twai_init()` initializes I2C, then sends commands to CH422G to switch `FSUSB42UMX` to CAN mode (`0x20` to `0x38`).
2. **TWAI Alert Monitoring**:
   - Enables alert flags: `TWAI_ALERT_TX_IDLE`, `TWAI_ALERT_TX_SUCCESS`, `TWAI_ALERT_TX_FAILED`, `TWAI_ALERT_ERR_PASS`, `TWAI_ALERT_BUS_ERROR`.
   - Polls alerts using `twai_read_alerts()` and inspects `twai_status_info_t`.

---

### 2.4 SD Card Mounting & File Handling
Located in [`sd_card.c`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/03_SD_Test/main/sd_card.c):

1. **SPI Mode Mounting**:
   - Mounts FATFS filesystem via `esp_vfs_fat_sdspi_mount`.
   - Configures `format_if_mount_failed` option to prevent silent data destruction unless explicitly enabled in Kconfig.
2. **CS Pin Toggling**:
   - Calls `ch422g_init_for_output()` prior to mounting to ensure CS line is low.

---

### 2.5 ADC Sensor Measurement & Calibration
Located in [`oneshot_read_main.c`](file:///e:/supersonic-sensor-ACLAB/reference/lcd-example/examples/ESP-IDF/04_Sensor_AD/main/oneshot_read_main.c):

1. **ADC Oneshot Unit API**:
   - Uses ESP-IDF v5 ADC Oneshot API (`adc_oneshot_new_unit`, `adc_oneshot_config_channel`).
2. **Curve Fitting Calibration**:
   - Checks eFuse calibration data via `adc_cali_create_scheme_curve_fitting`.
   - Converts raw ADC readings directly to millivolts (`adc_cali_raw_to_voltage`), avoiding uncalibrated linear approximation errors.

---

### 2.6 Edge Cases & Deficiencies Identified in Manufacturer Examples

| Deficiencies in Example Code | Production Impact / Risk | Recommended Fix for Project |
| :--- | :--- | :--- |
| **No I2C Bus Locking** | CH422G writes during GT911 touch reading cause I2C bus lockup. | Wrap all I2C transactions in a mutex lock. |
| **Missing CAN Bus-Off Recovery** | If TWAI bus enters Bus-Off state, transmission halts permanently. | Implement `twai_initiate_recovery()` on `TWAI_ALERT_BUS_OFF`. |
| **Blocking `vTaskDelay` in Main Loops** | Main loop polls with hardcoded delays (1000ms). | Replace with FreeRTOS queues & event groups. |
| **Raw `printf` logging instead of `ESP_LOG`** | Missing log level filtering and timestamps in TWAI examples. | Replace `printf` with `ESP_LOGI` / `ESP_LOGE`. |

---
*(Proceed to Session 2: LVGL `lv_demos` Architecture & Performance Audit)*
