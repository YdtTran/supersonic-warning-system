#pragma once
#include <stdint.h>

// =========================================================
// CẤU HÌNH KẾT NỐI COREIOT (ThingsBoard MQTT)
// Board này quản lý 2 cảm biến vật lý = S3 (left_front) và
// S5 (right_front) trong sơ đồ 6 cảm biến (xem
// firmware/waveshare-screen/README.md - Sensor Layout).
//
// Token dưới đây trùng với SENSOR_NODE_DEVICE_TOKEN trong
// config/keys.json (gitignored) - đổi cả 2 nơi nếu cấp lại token
// trên CoreIoT.
// =========================================================

#define COREIOT_WIFI_SSID "HCMUT-MEETING"
#define COREIOT_WIFI_PASS "hcmut@meeting"

#define COREIOT_MQTT_HOST "app.coreiot.io"
#define COREIOT_MQTT_PORT 1883
#define COREIOT_MQTT_TOKEN "OpXQiVBnETXAgVehd2Vg"
#define COREIOT_MQTT_CLIENT_ID "sensor-node-s3-s5"

#define COREIOT_TELEMETRY_TOPIC "v1/devices/me/telemetry"

// Tần suất publish lên CoreIoT (tách biệt với MEASURE_INTERVAL_MS
// trong Config.h - vòng lặp đo/lọc cục bộ vẫn giữ nguyên tốc độ).
static const uint32_t COREIOT_PUBLISH_INTERVAL_MS = 500;

// Khoảng cách tối thiểu giữa các lần thử kết nối lại MQTT khi mất kết nối.
static const uint32_t COREIOT_MQTT_RETRY_INTERVAL_MS = 3000;
