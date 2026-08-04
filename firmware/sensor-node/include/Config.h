#pragma once
#include <stdint.h>
#include <stddef.h>

// =========================================================
// KẾT NỐI PHẦN CỨNG - MẢNG CẢM BIẾN
// Mỗi phần tử là một cặp chân Trig/Echo độc lập. Thêm cảm biến
// mới bằng cách thêm một dòng {trig, echo} vào đây.
// =========================================================

struct SensorPinConfig
{
    uint8_t trigPin;
    uint8_t echoPin;
};

static const SensorPinConfig SENSOR_PINS[] = {
    {5, 6}, // Cảm biến 0
    {7, 8}, // Cảm biến 1
};

static const size_t SENSOR_COUNT = 2;

// =========================================================
// CẤU HÌNH CẢM BIẾN JSN-SR04T
// =========================================================

// Tốc độ âm thanh ở khoảng 20°C (cm / microsecond)
static const float SOUND_SPEED_CM_PER_US = 0.0343f;

// Phạm vi thực tế theo tài liệu JSN-SR04T
static const float MIN_DISTANCE_CM = 25.0f;
static const float MAX_DISTANCE_CM = 500.0f;

// 5 m cần khoảng 29.2 ms cho sóng đi và về.
// Đặt timeout 40 ms để có khoảng an toàn.
static const uint32_t ECHO_TIMEOUT_US = 40000;

// Thời gian chờ Echo trở về LOW trước lần đo mới
static const uint32_t ECHO_LOW_TIMEOUT_US = 5000;

// Trigger của JSN-SR04T V2 có thể cần dài hơn 10 us
static const uint32_t TRIGGER_LOW_US = 5;
static const uint32_t TRIGGER_HIGH_US = 20;

// Khoảng thời gian giữa hai lần đo
static const uint32_t MEASURE_INTERVAL_MS = 100;

// =========================================================
// CẤU HÌNH BỘ LỌC
// =========================================================

// Số mẫu RAW gần nhất được lưu lại
static const int HISTORY_SIZE = 9;

// Cần ít nhất 5 mẫu mới bắt đầu xuất kết quả
static const int MIN_SAMPLES_TO_FILTER = 5;

// Cụm hợp lệ phải chiếm ít nhất 5 mẫu
static const int MIN_CLUSTER_SIZE = 5;

// Dung sai nhỏ nhất để hai mẫu thuộc cùng một cụm
static const float BASE_CLUSTER_TOLERANCE_CM = 8.0f;

// Dung sai tăng theo khoảng cách
static const float CLUSTER_TOLERANCE_RATIO = 0.08f;

// EMA làm mượt kết quả cuối
static const float EMA_ALPHA = 0.30f;

// Nếu cụm mới lệch quá 30 cm hoặc 25% thì xem là bước nhảy lớn
static const float MIN_JUMP_THRESHOLD_CM = 30.0f;
static const float JUMP_THRESHOLD_RATIO = 0.25f;

// Bước nhảy phải được xác nhận nhiều lần
static const int JUMP_CONFIRM_COUNT = 3;

// Dung sai khi xác nhận ứng viên bước nhảy
static const float BASE_JUMP_TOLERANCE_CM = 12.0f;
static const float JUMP_TOLERANCE_RATIO = 0.10f;

// Sau nhiều lần đọc lỗi liên tiếp sẽ reset bộ lọc
static const int RESET_AFTER_INVALID = 15;

// =========================================================
// CẤU HÌNH BUZZER CẢNH BÁO
// Buzzer gắn trực tiếp trên sensor-node, kêu theo khoảng cách gần
// nhất trong số các cảm biến - cùng ngưỡng WARNING/DANGER với rule
// chain CoreIoT (server tính "relay"/"warning_status" cùng công thức
// để đồng bộ, xem cloud/coreiot/rule_chain/supersonic_rule_chain.json).
// =========================================================

static const uint8_t BUZZER_PIN = 48;

// <= ngưỡng này: WARNING (cùng điều kiện relay ON phía server)
static const float BUZZER_WARNING_DISTANCE_CM = 50.0f;

// < ngưỡng này: DANGER
static const float BUZZER_DANGER_DISTANCE_CM = 20.0f;

// Chu kỳ kêu: WARNING 3s/lần, DANGER 1s/lần
static const uint32_t BUZZER_WARNING_PERIOD_MS = 3000;
static const uint32_t BUZZER_DANGER_PERIOD_MS = 1000;

// Độ dài mỗi tiếng kêu (bật còi)
static const uint32_t BUZZER_BEEP_ON_MS = 120;
