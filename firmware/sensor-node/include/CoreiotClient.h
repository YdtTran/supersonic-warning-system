#pragma once
#include <stdint.h>

// Client MQTT mỏng để gửi telemetry lên CoreIoT (ThingsBoard).
// Không blocking: kết nối lại WiFi/MQTT được thử theo chu kỳ trong
// coreiotClientLoop(), không dùng delay() để không chặn networkTask.
class CoreiotClient {
public:
    // Khởi tạo WiFi STA + cấu hình MQTT client. Gọi 1 lần trong networkTask.
    void begin();

    // Gọi liên tục trong vòng lặp của networkTask: duy trì WiFi/MQTT,
    // xử lý PubSubClient::loop().
    void loop();

    bool isConnected() const;

    // Gửi payload JSON lên COREIOT_TELEMETRY_TOPIC. Trả về false nếu
    // chưa kết nối MQTT hoặc publish thất bại.
    bool publishTelemetry(const char *jsonPayload);

private:
    uint32_t _lastMqttRetryMs = 0;

    void ensureWifiConnected();
    void ensureMqttConnected();
};
