#include "SoftUART.h"

SoftUART::SoftUART() {
    _rxPin = -1;
    _txPin = -1;
    _baudRate = 9600;
    _bitTimeUs = 104;
}

SoftUART::SoftUART(int rxPin, int txPin, uint32_t baud) {
    begin(rxPin, txPin, baud);
}

void SoftUART::begin(int rxPin, int txPin, uint32_t baud) {
    _rxPin = rxPin;
    _txPin = txPin;
    _baudRate = baud;
    _bitTimeUs = 1000000 / baud; // 104us cho 9600 baud

    if (_rxPin >= 0) {
        pinMode(_rxPin, INPUT_PULLUP);
    }
    if (_txPin >= 0) {
        pinMode(_txPin, OUTPUT);
        digitalWrite(_txPin, HIGH); // Idle High cho UART TX
    }
}

bool SoftUART::readByte(uint8_t *outByte, uint32_t timeoutMs) {
    if (_rxPin < 0) return false;

    uint32_t startMs = millis();

    // 1. Chờ Start Bit (Đường truyền chuyển từ HIGH sang LOW)
    while (digitalRead(_rxPin) == HIGH) {
        if (millis() - startMs > timeoutMs) {
            return false; // Timeout nếu không có dữ liệu
        }
        ets_delay_us(2);
    }

    // 2. Chờ 1.5 bit time để đến giữa Bit 0 (Start bit duration 1T + 0.5T = 1.5T)
    ets_delay_us(_bitTimeUs + (_bitTimeUs / 2));

    uint8_t data = 0;

    // 3. Đọc 8 bit dữ liệu (LSB gửi trước)
    for (int i = 0; i < 8; i++) {
        if (digitalRead(_rxPin) == HIGH) {
            data |= (1 << i);
        }
        ets_delay_us(_bitTimeUs);
    }

    // 4. Chờ nửa Stop bit
    ets_delay_us(_bitTimeUs / 2);

    *outByte = data;
    return true;
}

bool SoftUART::readSensorPacket(uint16_t *distance_mm, uint32_t timeoutMs) {
    uint8_t rxBuffer[4];
    uint32_t startMs = millis();

    // Tìm Byte Header (0xFF)
    while (millis() - startMs < timeoutMs) {
        uint8_t b;
        if (readByte(&b, 30)) {
            if (b == 0xFF) {
                rxBuffer[0] = b;
                
                // Đọc tiếp 3 byte: Data_H, Data_L, Checksum
                if (readByte(&rxBuffer[1], 30) &&
                    readByte(&rxBuffer[2], 30) &&
                    readByte(&rxBuffer[3], 30)) {
                    
                    uint8_t calcChecksum = (rxBuffer[0] + rxBuffer[1] + rxBuffer[2]) & 0xFF;
                    if (calcChecksum == rxBuffer[3]) {
                        *distance_mm = (rxBuffer[1] << 8) | rxBuffer[2];
                        return true; // Giải mã thành công
                    }
                }
            }
        }
    }
    return false;
}

void SoftUART::writeByte(uint8_t data) {
    if (_txPin < 0) return;

    // Start Bit (LOW)
    digitalWrite(_txPin, LOW);
    ets_delay_us(_bitTimeUs);

    // 8 Data Bits
    for (int i = 0; i < 8; i++) {
        digitalWrite(_txPin, (data >> i) & 0x01);
        ets_delay_us(_bitTimeUs);
    }

    // Stop Bit (HIGH)
    digitalWrite(_txPin, HIGH);
    ets_delay_us(_bitTimeUs);
}
