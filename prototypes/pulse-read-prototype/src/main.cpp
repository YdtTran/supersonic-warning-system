/*
  ĐỌC XUNG (PULSE) TỪ SR04M-2 - CHẾ ĐỘ TRIGGER/ECHO (MODE 3)
  --------------------------------------------------------------
  Khác với water-level-uart (đọc khung UART Mode 1 sensor tự gửi),
  prototype này dùng GPIO trig/echo thuần (không UART):
    - Kéo chân RX của sensor xuống LOW >= 20us để kích đo (trigger).
    - Sensor phát 8 xung 40kHz rồi trả về 1 xung HIGH trên chân TX,
      độ rộng xung tỉ lệ với khoảng cách đo được (echo).
    - distance_cm = pulse_width_us / 58.0  (tương đương v_sound=340m/s, khứ hồi)

  Đấu nối (Yolo Uno - ESP32-S3):
    SR04M-2 RX  -> GPIO5  (Yolo Uno OUTPUT, trigger)
    SR04M-2 TX  -> GPIO6  (Yolo Uno INPUT, echo)
    SR04M-2 VCC -> 5V
    SR04M-2 GND -> GND

  Board test qua cổng COM10 (xem platformio.ini: upload_port/monitor_port).

  Lưu ý: nếu sensor của bạn trigger bằng mức HIGH (thay vì LOW) hoặc idle
  state khác, đổi TRIGGER_ACTIVE_LOW bên dưới cho khớp datasheet thực tế.
*/

#include <Arduino.h>

// ----------------- CẤU HÌNH CHÂN -----------------
#define TRIG_PIN 6 // nối vào chân RX của SR04M-2
#define ECHO_PIN 5 // nối vào chân TX của SR04M-2

#define TRIGGER_ACTIVE_LOW true // Mode 3 của SR04M-2: kích đo bằng xung LOW
#define TRIGGER_PULSE_US 25     // độ rộng xung trigger (datasheet yêu cầu >= 20us)
#define ECHO_TIMEOUT_US 30000UL // timeout chờ echo (~5m tương ứng ~29ms khứ hồi)

const float DIST_MIN_CM = 20.0;
const float DIST_MAX_CM = 600.0;

// ----------------- KÍCH XUNG TRIGGER -----------------
void sendTrigger()
{
    if (TRIGGER_ACTIVE_LOW)
    {
        digitalWrite(TRIG_PIN, HIGH); // idle HIGH
        delayMicroseconds(2);
        digitalWrite(TRIG_PIN, LOW);
        delayMicroseconds(TRIGGER_PULSE_US);
        digitalWrite(TRIG_PIN, HIGH);
    }
    else
    {
        digitalWrite(TRIG_PIN, LOW); // idle LOW
        delayMicroseconds(2);
        digitalWrite(TRIG_PIN, HIGH);
        delayMicroseconds(TRIGGER_PULSE_US);
        digitalWrite(TRIG_PIN, LOW);
    }
}

// ----------------- ĐỌC XUNG ECHO (RAW, TỰ POLL GPIO) -----------------
// Không dùng pulseIn() (black-box, chỉ trả về 1 số hoặc 0 khi timeout).
// Tự bắt cạnh lên/xuống bằng digitalRead()+micros() để biết chính xác
// đang timeout ở giai đoạn nào (chờ rising hay chờ falling), phục vụ debug.
struct RawEcho
{
    bool level_before;      // mức chân ECHO ngay truoc khi trigger
    bool got_rising;        // co bat duoc canh len khong
    bool got_falling;       // co bat duoc canh xuong khong (sau khi da len)
    unsigned long pulse_us; // do rong xung tho (chi hop le khi got_rising && got_falling)
};

RawEcho readRawEcho()
{
    RawEcho r;
    r.level_before = digitalRead(ECHO_PIN);
    r.got_rising = false;
    r.got_falling = false;
    r.pulse_us = 0;

    sendTrigger();

    unsigned long waitStart = micros();
    // Cho canh len (bat dau xung echo)
    while (digitalRead(ECHO_PIN) == LOW)
    {
        if (micros() - waitStart > ECHO_TIMEOUT_US)
        {
            return r; // timeout: khong bao gio len HIGH
        }
    }
    r.got_rising = true;
    unsigned long riseTime = micros();

    // Cho canh xuong (ket thuc xung echo)
    while (digitalRead(ECHO_PIN) == HIGH)
    {
        if (micros() - riseTime > ECHO_TIMEOUT_US)
        {
            return r; // timeout: len roi nhung khong bao gio xuong
        }
    }
    r.got_falling = true;
    r.pulse_us = micros() - riseTime;

    return r;
}

// Nghe thu dong ECHO_PIN mot khoang thoi gian ma KHONG trigger, de kiem tra
// xem sensor co dang tu dong doi muc (kieu UART idle-high, chi LOW ngan khi
// gui start bit) hay khong - giup phan biet "sensor dang o Mode 1 UART" voi
// "chan that su khong ket noi/luon HIGH".
void passiveListenDiag(unsigned long durationMs)
{
    Serial.println("=== Passive listen ECHO_PIN (khong trigger) ===");
    unsigned long t0 = millis();
    bool lastLevel = digitalRead(ECHO_PIN);
    int transitions = 0;
    unsigned long minLowUs = 0xFFFFFFFF;
    unsigned long lastEdgeUs = micros();

    while (millis() - t0 < durationMs)
    {
        bool level = digitalRead(ECHO_PIN);
        if (level != lastLevel)
        {
            unsigned long now = micros();
            unsigned long widthUs = now - lastEdgeUs;
            if (lastLevel == LOW && widthUs < minLowUs)
            {
                minLowUs = widthUs;
            }
            transitions++;
            lastEdgeUs = now;
            lastLevel = level;
        }
    }

    Serial.print("transitions=");
    Serial.print(transitions);
    Serial.print(" min_low_width_us=");
    Serial.println(transitions > 0 ? minLowUs : 0);
    Serial.println("=== Het passive listen ===");
}

void setup()
{
    Serial.begin(115200);

    pinMode(TRIG_PIN, OUTPUT);
    digitalWrite(TRIG_PIN, TRIGGER_ACTIVE_LOW ? HIGH : LOW); // idle state

    // Test 1: INPUT thuong (khong pull) - xem gia tri mac dinh
    pinMode(ECHO_PIN, INPUT);
    delay(5);
    Serial.print("ECHO_PIN voi INPUT (no pull) = ");
    Serial.println(digitalRead(ECHO_PIN) ? "HIGH" : "LOW");

    // Test 2: ep INPUT_PULLDOWN - neu van doc HIGH nghia la co cai gi do
    // dang thuc su keo len HIGH (driven). Neu tut xuong LOW nghia la chan
    // dang floating (khong co gi ben ngoai dieu khien no).
    pinMode(ECHO_PIN, INPUT_PULLDOWN);
    delay(5);
    Serial.print("ECHO_PIN voi INPUT_PULLDOWN = ");
    Serial.println(digitalRead(ECHO_PIN) ? "HIGH (co tin hieu ngoai keo len)" : "LOW (chan dang floating, khong co gi dieu khien)");

    pinMode(ECHO_PIN, INPUT);

    Serial.println("Bat dau doc xung raw (pulse) tu SR04M-2 qua GPIO Trig/Echo");
    passiveListenDiag(500);
}

void loop()
{
    RawEcho r = readRawEcho();

    // Luon in du lieu tho truoc, khong am parse/loc gi ca
    Serial.print("echo_before=");
    Serial.print(r.level_before ? "HIGH" : "LOW");
    Serial.print(" rising=");
    Serial.print(r.got_rising ? "yes" : "NO");
    Serial.print(" falling=");
    Serial.print(r.got_falling ? "yes" : "NO");
    Serial.print(" pulse_us=");
    Serial.print(r.pulse_us);

    if (r.got_rising && r.got_falling)
    {
        float distance_cm = r.pulse_us / 58.0f;
        Serial.print(" -> distance=");
        Serial.print(distance_cm, 1);
        Serial.print("cm");
        if (distance_cm < DIST_MIN_CM || distance_cm > DIST_MAX_CM)
        {
            Serial.print(" (out of range)");
        }
    }
    Serial.println();

    delay(100); // SR04M-2 khuyen nghi >= 60ms giua 2 lan trigger
}
