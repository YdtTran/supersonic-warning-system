#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Kết quả một lần đọc RAW từ cảm biến
struct SensorReading {
    uint32_t durationUs;   // độ rộng xung Echo (us), 0 nếu chưa có
    float distanceCm;      // khoảng cách tính được (chỉ có ý nghĩa nếu error == nullptr,
                            // hoặc khi lỗi là "vượt phạm vi")
    const char* error;     // nullptr nếu mẫu hợp lệ, ngược lại là lý do reject
};

// Một cảm biến JSN-SR04T độc lập (1 cặp chân Trig/Echo + 1 ngắt riêng).
// Tạo nhiều instance (mảng tĩnh, không dùng std::vector) để đọc song song
// nhiều cảm biến, mỗi instance tự quản lý trạng thái ISR/semaphore của mình.
class UltrasonicSensor {
public:
    // Khởi tạo chân Trigger/Echo + ngắt ngoài + semaphore.
    // Gọi 1 lần trong task đọc cảm biến trước khi dùng readOnce().
    void begin(uint8_t trigPin, uint8_t echoPin);

    // Phát Trigger và đo Echo.
    // KHÔNG busy-wait: dùng ngắt phần cứng bắt cạnh Echo + xSemaphoreTake(timeout),
    // nên trong lúc chờ, FreeRTOS scheduler vẫn chạy các task khác bình thường.
    // Chỉ nên gọi hàm này từ MỘT task cho MỘT instance tại một thời điểm.
    SensorReading readOnce();

private:
    uint8_t _trigPin = 0;
    uint8_t _echoPin = 0;
    SemaphoreHandle_t _echoSemaphore = nullptr;

    // Trạng thái dùng trong ISR - phải là volatile, riêng cho từng instance
    volatile uint32_t _echoRiseUs = 0;
    volatile uint32_t _echoDurationUs = 0;

    bool waitEchoLow(uint32_t timeoutUs);

    // ISR tĩnh dùng chung cho mọi instance, nhận lại instance qua attachInterruptArg
    // (thay cho std::function/lambda để không kéo theo STL).
    static void IRAM_ATTR onEchoChange(void *arg);
};
