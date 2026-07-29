# Waveshare Hardware Peripheral Examples Audit Reference

This reference document summarizes the audit results of manufacturer hardware examples for the Waveshare ESP32-S3 Touch LCD panel.

---

## 1. Peripherals & Driver Matrix

| Module | Primary Hardware IC | ESP-IDF Drivers / Interfaces | Key Architectural Findings |
| :--- | :--- | :--- | :--- |
| **`01_I2C_Test`** | CH422G / GT911 | `driver/i2c_master.h` | Console REPL scanner. Establishes I2C bus address detection (`0x24` for CH422G, `0x5D` for GT911). |
| **`02_RS485_Test`** | MAX13487 / SP3485 | `driver/uart.h` | Half-duplex RS485 communication with auto-direction hardware flow control. |
| **`03_SD_Test`** | SD Card Slot | `driver/sdspi_host.h`, `fatfs` | SD Card SPI mode. CS line controlled via CH422G IO Expander bit 4. |
| **`04_Sensor_AD`** | Analog Distance Sensor | `esp_adc/adc_oneshot.h` | Oneshot ADC sampling combined with polynomial curve-fitting calibration for accurate distance readings. |
| **`05_UART_Test`** | Serial Echo | `driver/uart.h` | Dedicated UART RX/TX FreeRTOS background task with ringbuffer processing. |
| **`06_TWAItransmit`** | CAN Bus Transceiver | `driver/twai.h`, CH422G | CAN Bus packet transmission. FSUSB42UMX USB/CAN MUX toggled via CH422G. |
| **`07_TWAIreceive`** | CAN Bus Transceiver | `driver/twai.h` | CAN Bus packet reception with ISR alert handling (`TWAI_ALERT_RECEIVE_QUEUE_FULL`). |
| **`08_lvgl_v8_demo`** | 7" 800x480 RGB LCD | `esp_lcd`, `lvgl` v8 | RGB LCD panel init with PSRAM framebuffers (`.fb_in_psram = 1`), GT911 touch reset via CH422G pulse sequence. |
| **`09_lvgl_v9_demo`** | 7" 800x480 RGB LCD | `esp_lcd`, `lvgl` v9 | LVGL v9 migration driver using `esp_lv_adapter` component, SRAM DMA bounce buffer (`800 * 40` px). |

---

## 2. Key Hardware Gotchas & Best Practices

1. **CH422G IO Expander Integration**:
   - CH422G controls LCD backlight (bit 0), GT911 touch reset (bit 1), SD card CS (bit 4), and CAN MUX (bit 5).
   - Write `0x1E` to register `0x38` during boot to enable LCD backlight.
2. **I2C Shared Bus Mutex**:
   - Both GT911 and CH422G reside on `I2C_NUM_0`. Thread-safe locking (`xI2C_Mutex`) is strictly required to avoid I2C bus collision during simultaneous touch reads and IO expander writes.
3. **PSRAM Framebuffers & SRAM Bounce Buffer**:
   - Dual 800x480 RGB565 framebuffers (~1.5MB) are placed in PSRAM.
   - Internal SRAM bounce buffer (`800 * 40` px) prevents screen tearing during active Wi-Fi and MQTT background operations.
