/*
  ĐO MỰC NƯỚC BẰNG JSN-SR04T-V3 (UART MODE 1) + KALMAN FILTER
  --------------------------------------------------------------
  - Sensor lắp ở độ cao H_SENSOR_MM so với đáy bể / mặt đất.
  - Sensor đo khoảng cách từ nó đến mặt nước (d_raw).
  - Mực nước = H_SENSOR_MM - d_filtered (d_filtered đã qua Kalman).

  Khung UART Mode 1 (4 byte, baud 9600, 8N1):
    [0] Header   = 0xFF
    [1] Data_H
    [2] Data_L
    [3] Checksum = (Header + Data_H + Data_L) & 0xFF
    distance_mm  = (Data_H << 8) | Data_L

  Đấu nối (ví dụ ESP32):
    JSN-SR04T-V3 TX  -> ESP32 RX2 (GPIO44)
    JSN-SR04T-V3 RX  -> ESP32 TX2 (GPIO43)   (không bắt buộc dùng ở mode tự động gửi)
    JSN-SR04T-V3 VCC -> 5V
    JSN-SR04T-V3 GND -> GND

  Nếu dùng Arduino Uno/Nano: dùng SoftwareSerial thay cho Serial2.
*/

#include <Arduino.h>
#include "SoftUART.h"

// ----------------- CẤU HÌNH -----------------
#define SENSOR_RX_PIN 38 // chân RX (đọc mềm) của ESP32 nối vào TX sensor
#define SENSOR_TX_PIN -1 // không dùng (sensor tự gửi khung, không cần trigger)
#define SENSOR_BAUD 9600

const float H_SENSOR_MM = 1500.0; // độ cao lắp sensor so với đáy (mm) = 1.5m

// Giới hạn hợp lệ của JSN-SR04T-V3 (theo datasheet, có thể chỉnh theo thực tế)
const float DIST_MIN_MM = 250.0;
const float DIST_MAX_MM = 7500.0;

// Tham số Kalman filter (chỉnh theo thực nghiệm của bạn)
float Q = 4.0;   // process noise: mực nước biến đổi chậm -> để nhỏ (mm^2)
float R = 100.0; // measurement noise: nhiễu cảm biến (mm^2), càng rung/gợn sóng thì để cao hơn

// ----------------- BIẾN KALMAN -----------------
float kf_x = 0;    // ước lượng khoảng cách hiện tại (state)
float kf_p = 1000; // ước lượng sai số (covariance), khởi tạo lớn vì chưa tin tưởng
bool kf_initialized = false;

SoftUART SensorSerial; // UART mềm (bit-bang), tránh trùng chân UART0 (GPIO43/44)

// ----------------- HÀM KALMAN 1D -----------------
float kalmanUpdate(float measurement)
{
    if (!kf_initialized)
    {
        // Lần đầu tiên: khởi tạo state = giá trị đo đầu tiên
        kf_x = measurement;
        kf_p = R;
        kf_initialized = true;
        return kf_x;
    }

    // ---- Bước dự đoán (predict) ----
    // Mô hình tĩnh: giả định khoảng cách không đổi nhiều giữa 2 lần đo
    float x_pred = kf_x;
    float p_pred = kf_p + Q;

    // ---- Bước cập nhật (update) ----
    float K = p_pred / (p_pred + R); // Kalman gain
    kf_x = x_pred + K * (measurement - x_pred);
    kf_p = (1.0 - K) * p_pred;

    return kf_x;
}

// ----------------- ĐỌC KHUNG UART TỪ CẢM BIẾN -----------------
// Trả về true nếu đọc được 1 khung hợp lệ, giá trị mm ghi vào distance_mm
bool readSensorFrame(float &distance_mm)
{
    uint16_t raw_mm = 0;
    // readSensorPacket() tự tìm header 0xFF, đọc đủ 4 byte và kiểm checksum
    if (!SensorSerial.readSensorPacket(&raw_mm, 100))
    {
        return false;
    }

    // Lọc theo khoảng đo hợp lệ của sensor
    if (raw_mm < DIST_MIN_MM || raw_mm > DIST_MAX_MM)
    {
        return false;
    }

    distance_mm = (float)raw_mm;
    return true;
}

void setup()
{
    Serial.begin(115200);
    SensorSerial.begin(SENSOR_RX_PIN, SENSOR_TX_PIN, SENSOR_BAUD);

    Serial.println("Bat dau do muc nuoc voi JSN-SR04T-V3 (UART Mode 1) + Kalman filter");
}

void loop()
{
    float distance_raw_mm;

    if (readSensorFrame(distance_raw_mm))
    {
        // Áp Kalman filter lên khoảng cách đo được (trước khi tính mực nước)
        float distance_filtered_mm = kalmanUpdate(distance_raw_mm);

        // Tính mực nước = độ cao lắp sensor - khoảng cách đến mặt nước
        // float water_level_mm = H_SENSOR_MM - distance_filtered_mm;
        float water_level_mm = distance_filtered_mm;

        if (water_level_mm < 0)
            water_level_mm = 0; // tránh giá trị âm do nhiễu

        // float water_level_raw_mm = H_SENSOR_MM - distance_raw_mm;
        float water_level_raw_mm = distance_raw_mm;

        Serial.print("Raw dist: ");
        Serial.print(distance_raw_mm, 1);
        Serial.print(" mm | Filtered dist: ");
        Serial.print(distance_filtered_mm, 1);
        // Serial.print(" mm | Water level (raw): ");
        // Serial.print(water_level_raw_mm, 1);
        // Serial.print(" mm | Water level (Kalman): ");
        // Serial.print(water_level_mm, 1);
        // Serial.println(" mm");
    }

    delay(20); // tốc độ vòng lặp đọc, sensor tự gửi khung ~100-140ms/lần
}