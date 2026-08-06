#pragma once
#include <stdint.h>
#include "Config.h"

// =========================================================
// CẤU HÌNH ESP-NOW (giao tiếp trực tiếp sensor-node -> waveshare-screen)
// Đường truyền cục bộ, tách biệt khỏi luồng MQTT/CoreIoT. Nguồn thông
// tin dùng chung (MAC, channel, ý nghĩa struct) xem
// docs/architecture/ESPNOW_NETWORK.md - struct bên dưới phải khớp hệt
// bản định nghĩa trong firmware/waveshare-screen/src/main.c.
// =========================================================

// Cả 2 board phải cùng WiFi channel cố định này.
static const uint8_t ESPNOW_CHANNEL = 1;

// MAC của waveshare-screen (receiver) - đích mà sensor-node gửi tới.
// Cập nhật theo docs/architecture/ESPNOW_NETWORK.md nếu board waveshare-screen đổi.
static const uint8_t ESPNOW_PEER_MAC[6] = {0x64, 0xe8, 0x33, 0x7c, 0x3f, 0xe0};

// Tần suất gửi ESP-NOW (tách biệt với MEASURE_INTERVAL_MS trong Config.h -
// vòng lặp đo/lọc cục bộ vẫn giữ nguyên tốc độ).
static const uint32_t ESPNOW_SEND_INTERVAL_MS = 500;

// Số vị trí cảm biến trong message - khớp SENSOR_MODEL_COUNT bên
// waveshare-screen (sensor_model.h), không phải SENSOR_COUNT (Config.h,
// chỉ số cảm biến vật lý thực có trên board này).
#define ESPNOW_SENSOR_SLOT_COUNT 6

// idx theo sensor_id_t bên waveshare-screen:
// 0=front,1=rear,2=left_front,3=left_rear,4=right_front,5=right_rear
#define ESPNOW_SLOT_FRONT 0
#define ESPNOW_SLOT_REAR 1
#define ESPNOW_SLOT_LEFT_FRONT 2
#define ESPNOW_SLOT_LEFT_REAR 3
#define ESPNOW_SLOT_RIGHT_FRONT 4
#define ESPNOW_SLOT_RIGHT_REAR 5

// Payload nhị phân gửi qua ESP-NOW - packed, kích thước cố định. Luôn gửi
// đủ ESPNOW_SENSOR_SLOT_COUNT (6) vị trí; valid[i]=0 nghĩa là "null" cho
// slot đó (cảm biến lỗi/mất tín hiệu hoặc chưa lắp phần cứng) - phía
// waveshare-screen phải tự xử lý hiển thị khi valid[i]=0, không được coi
// distance_cm[i] là số đọc hợp lệ.
typedef struct __attribute__((packed))
{
    float distance_cm[ESPNOW_SENSOR_SLOT_COUNT];
    uint8_t valid[ESPNOW_SENSOR_SLOT_COUNT];
} espnow_sensor_msg_t;

// Ánh xạ theo đúng thứ tự vật lý trong SENSOR_PINS (Config.h) sang slot
// ESP-NOW tương ứng bên waveshare-screen. Cập nhật mảng này (và comment)
// mỗi khi thêm/bớt cảm biến trong Config.h::SENSOR_PINS.
static const uint8_t SENSOR_ESPNOW_SLOT[SENSOR_COUNT] = {
    ESPNOW_SLOT_FRONT,       // Cảm biến FRONT (5,6)
    ESPNOW_SLOT_LEFT_FRONT,  // Cảm biến LR (7,8)
    ESPNOW_SLOT_RIGHT_FRONT, // Cảm biến RF (9,10)
};
