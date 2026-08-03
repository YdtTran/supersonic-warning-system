# Sensor Node - Version Log & History

Target Module: `sensor-node/`  
Microcontroller: ESP32-S3 (Dual-Core 240MHz, Wi-Fi, BT 5 LE)  
Default Port: `COM8`  
Software Environment: ESP-IDF `v6.0.2`, GCC `15.2.0`  

---

## Version Releases

### [v1.0.0] - 2026-07-28
#### Added
- Base sensor node project setup targeted for ESP32-S3.
- Integrated peripheral drivers (LEDC PWM, GPIO, SPI/I2C sensors).
- Incremental build & flash support via `build_and_flash.bat` (Default Port: `COM8`).
