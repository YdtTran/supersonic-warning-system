#ifndef SOFT_UART_H
#define SOFT_UART_H

#include <Arduino.h>

class SoftUART {
private:
    int _rxPin;
    int _txPin;
    uint32_t _baudRate;
    uint32_t _bitTimeUs;

public:
    SoftUART();
    SoftUART(int rxPin, int txPin = -1, uint32_t baud = 9600);

    void begin(int rxPin, int txPin = -1, uint32_t baud = 9600);
    
    // Đọc 1 byte theo chuẩn UART (8N1) với timeout (ms)
    bool readByte(uint8_t *outByte, uint32_t timeoutMs = 50);

    // Đọc gói tin 4-byte từ cảm biến siêu âm (Header 0xFF + DataH + DataL + Checksum)
    bool readSensorPacket(uint16_t *distance_mm, uint32_t timeoutMs = 100);

    // Gửi 1 byte (Software TX)
    void writeByte(uint8_t data);
};

#endif // SOFT_UART_H
