#pragma once
#include <stddef.h>

// Khởi tạo mutex bảo vệ dữ liệu dùng chung. Gọi 1 lần trong setup().
void sharedStateInit();

// Ghi khoảng cách ổn định mới nhất của cảm biến sensorIndex (gọi từ SensorTask)
void sharedStateSet(size_t sensorIndex, float distanceCm, bool valid);

// Đọc khoảng cách ổn định gần nhất của cảm biến sensorIndex (gọi từ bất kỳ
// task nào khác, vd AppTask). Trả về true nếu giá trị hợp lệ.
bool sharedStateGet(size_t sensorIndex, float &distanceCm);
