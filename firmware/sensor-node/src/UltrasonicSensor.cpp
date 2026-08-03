#include "UltrasonicSensor.h"
#include "Config.h"

void UltrasonicSensor::begin(uint8_t trigPin, uint8_t echoPin) {
    _trigPin = trigPin;
    _echoPin = echoPin;

    pinMode(_trigPin, OUTPUT);
    digitalWrite(_trigPin, LOW);
    pinMode(_echoPin, INPUT);

    _echoSemaphore = xSemaphoreCreateBinary();

    // arg = this: ISR tĩnh dùng lại instance đúng của cảm biến này
    attachInterruptArg(digitalPinToInterrupt(_echoPin), &UltrasonicSensor::onEchoChange, this, CHANGE);
}

// ISR: chạy trên cả cạnh lên và cạnh xuống của Echo (CHANGE).
// Không gọi bất kỳ hàm blocking/Serial nào trong ISR.
void IRAM_ATTR UltrasonicSensor::onEchoChange(void *arg) {
    UltrasonicSensor *self = static_cast<UltrasonicSensor *>(arg);

    if (digitalRead(self->_echoPin) == HIGH) {
        // Cạnh lên: bắt đầu đếm giờ
        self->_echoRiseUs = micros();
    } else {
        // Cạnh xuống: tính độ rộng xung và báo cho task đang chờ
        self->_echoDurationUs = micros() - self->_echoRiseUs;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(self->_echoSemaphore, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

// Chờ Echo trở về LOW trước khi phát Trigger mới.
// Dùng vTaskDelay(1 tick) thay vì busy-wait: task này nhường CPU
// cho các task khác trong lúc chờ, không làm "đứng" toàn hệ thống.
bool UltrasonicSensor::waitEchoLow(uint32_t timeoutUs) {
    uint32_t startMs = millis();
    uint32_t timeoutMs = (timeoutUs / 1000) + 1;

    while (digitalRead(_echoPin) == HIGH) {
        if (millis() - startMs >= timeoutMs) {
            return false;
        }
        vTaskDelay(1);
    }
    return true;
}

SensorReading UltrasonicSensor::readOnce() {
    SensorReading reading;
    reading.durationUs = 0;
    reading.distanceCm = 0.0f;
    reading.error = nullptr;

    // Echo phải ở LOW trước khi phát Trigger mới
    if (!waitEchoLow(ECHO_LOW_TIMEOUT_US)) {
        reading.error = "Echo bi giu HIGH truoc khi Trigger";
        return reading;
    }

    // Xóa mọi tín hiệu semaphore còn sót lại từ lần đo trước
    xSemaphoreTake(_echoSemaphore, 0);

    // Đưa Trigger về LOW trước, rồi phát xung HIGH 20 us.
    digitalWrite(_trigPin, LOW);
    delayMicroseconds(TRIGGER_LOW_US);

    digitalWrite(_trigPin, HIGH);
    delayMicroseconds(TRIGGER_HIGH_US);
    digitalWrite(_trigPin, LOW);

    // Chờ ISR báo đã bắt được xung Echo, có timeout.
    // xSemaphoreTake ở đây KHÔNG busy-wait: task bị block/put to sleep,
    // CPU được nhường cho các task khác chạy song song.
    TickType_t timeoutTicks = pdMS_TO_TICKS((ECHO_TIMEOUT_US / 1000) + 1);

    if (xSemaphoreTake(_echoSemaphore, timeoutTicks) != pdTRUE) {
        reading.error = "Khong nhan duoc Echo hop le (timeout)";
        return reading;
    }

    uint32_t durationUs = _echoDurationUs;
    reading.durationUs = durationUs;

    float distanceCm = durationUs * SOUND_SPEED_CM_PER_US / 2.0f;
    reading.distanceCm = distanceCm;

    // Loại vùng mù
    if (distanceCm < MIN_DISTANCE_CM) {
        reading.error = "Khoang cach duoi vung do 25 cm";
        return reading;
    }

    // Loại giá trị vượt phạm vi
    if (distanceCm > MAX_DISTANCE_CM) {
        reading.error = "Khong co Echo hop le hoac vuot 450 cm";
        return reading;
    }

    return reading;
}
