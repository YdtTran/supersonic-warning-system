# Sensor Node - GPIO47/48 PSRAM Conflict & ESP-NOW Slot Mapping Fix Log

Target Module: `sensor-node/` (sender) + `waveshare-screen/` (receiver)
Microcontroller: ESP32-S3 (Embedded PSRAM 8MB, xác nhận qua `esptool flash_id`)
Board: `yolo_uno` (PlatformIO, Arduino framework, sensor-node) / `yolo_uno` (PlatformIO, ESP-IDF, waveshare-screen)
Serial Port sensor-node: `COM18` (native USB CDC) | waveshare-screen: `COM9`

---

## 🎯 Vấn đề ban đầu

Sau khi mở rộng `sensor-node` từ 3 lên 6 cảm biến JSN-SR04T (thêm `L REAR`, `R REAR`, `REAR`) và dời `BUZZER_PIN` từ GPIO 48 sang GPIO 11 (tránh trùng chân), cảm biến thứ 6 ("REAR", ban đầu đấu ở GPIO 47/48 — gọi tắt là "node 6") **không hiển thị dữ liệu trên dashboard `waveshare-screen`**, dù đèn tín hiệu trên module cảm biến vẫn nháy bình thường (module vẫn trigger/nhận echo về mặt vật lý).

---

## 🔍 Quá trình chẩn đoán

### 1. Rà lại lịch sử git `Config.h`

Phát hiện `BUZZER_PIN` từng là **GPIO 48** — trùng thẳng với echo pin `{47, 48}` của cảm biến REAR mới thêm. Đây là nghi vấn đầu tiên, đã được người dùng tự sửa (chuyển buzzer sang GPIO 11) trước khi bắt đầu phiên debug này.

### 2. Lỗi build chặn trước khi debug được tiếp

`firmware/waveshare-screen/components/sensor_model/sensor_model.c` có một ký tự `x` lạc ở đầu file (`x /*...`) khiến không build được. Đã xoá.

### 3. Lỗi ánh xạ slot ESP-NOW (bug thật, độc lập với vấn đề GPIO)

`SENSOR_PINS[]` trong `Config.h` liệt kê theo thứ tự vật lý **FRONT, LR, RF, L REAR, R REAR, REAR**, nhưng `SENSOR_ESPNOW_SLOT[]` trong `EspNowConfig.h` lại gán tuần tự **FRONT, REAR, LEFT_FRONT, LEFT_REAR, RIGHT_FRONT, RIGHT_REAR** — lệch pha từ vị trí thứ 2 trở đi. Sửa lại đúng theo comment vật lý của từng phần tử.

### 4. Bật lại log debug REJECT + đọc serial trực tiếp qua `pio device monitor -p COM18`

Sau khi sửa 2 lỗi trên và reflash, log serial cho thấy:

```text
[S5] REJECT: Khong nhan duoc Echo hop le (timeout) | Pulse: 0 us | ... | Invalid: 1..15
```

**100% các lần đo của `[S5]` (REAR) đều `Pulse: 0 us`** — ISR `onEchoChange` chưa từng chạy trọn 1 chu kỳ rise→fall, trong khi `[S3]`/`[S4]` (GPIO 17/18, 21/38) vẫn nhận Echo bình thường (chỉ bị loại vì vượt 450cm — đúng nghĩa "không có vật cản trong tầm", không phải lỗi).

### 5. Xác nhận nguyên nhân gốc bằng `esptool flash_id`

```text
Chip type:  ESP32-S3 (QFN56) (revision v0.2)
Features:   Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, Embedded PSRAM 8MB (AP_3v3)
```

Chip này có **Embedded PSRAM 8MB**. Trên các biến thể ESP32-S3 có Octal PSRAM tích hợp sẵn, **GPIO 47/48 bị chip dùng nội bộ làm chân clock vi sai `SPICLK_P_DIFF`/`SPICLK_N_DIFF` cho PSRAM** — không hoạt động được như GPIO số thông thường, bất kể firmware cấu hình gì. Đây chính là lý do Echo không bao giờ tới được GPIO matrix dù cảm biến vẫn hoạt động vật lý bình thường.

---

## 🛠️ Chi tiết thay đổi mã nguồn

- **`firmware/waveshare-screen/components/sensor_model/sensor_model.c`**
  Xoá ký tự `x` lạc đầu file (lỗi build).

- **`firmware/sensor-node/include/EspNowConfig.h`**
  Sửa `SENSOR_ESPNOW_SLOT[]` cho khớp đúng thứ tự vật lý trong `SENSOR_PINS[]` (FRONT→FRONT, LR→LEFT_FRONT, RF→RIGHT_FRONT, L REAR→LEFT_REAR, R REAR→RIGHT_REAR, REAR→REAR).

- **`firmware/sensor-node/include/Config.h`**
  - `BUZZER_PIN`: `48` → `11` (tránh trùng echo pin cảm biến REAR).
  - Cảm biến REAR: `{47, 48}` → `{3, 4}` (né hoàn toàn vùng GPIO bị PSRAM chiếm dụng).

- **`firmware/sensor-node/src/main.cpp`**
  - Bật lại log debug `[S%u] REJECT: ...` (đã có sẵn nhưng bị comment tắt) để phục vụ chẩn đoán trực tiếp qua serial.
  - Thêm bảng `RESERVED_PINS[]` + hàm `warnIfReservedPin()`: in cảnh báo ngay lúc boot (`sensorTask`) nếu bất kỳ chân Trig/Echo/Buzzer nào trùng các GPIO đã biết là không dùng được trên board này (47/48 PSRAM diff-clock, 26-32 SPI0 flash/PSRAM, 19/20 USB D-/D+) — phát hiện sớm loại lỗi này ở lần cấu hình sau, không cần lặp lại quy trình bắt log REJECT + `esptool flash_id` từ đầu.

- **`firmware/waveshare-screen/components/ui_dashboard/ui_dashboard.c`**
  - Nút "Mute Alarm" đổi nhãn `Mute Alarm` ↔ `Unmute Alarm` theo đúng trạng thái, đổi màu nền khi đang mute (trước đó nhãn cố định, không biết đang bật/tắt).
  - Banner `OVERALL` hiện `OVERALL: DANGER (MUTED)` khi đang nguy hiểm mà bị mute, thay vì im lặng không dấu hiệu.

---

## 🔔 Phát hiện bổ sung: `buzzerTask` chưa bao giờ chạy

Khi rà soát lại toàn bộ project để đối chiếu tài liệu, phát hiện thêm một lỗi
độc lập giải thích vì sao chức năng còi cảnh báo "hoạt động mơ hồ":

`buzzerTask()` trong `firmware/sensor-node/src/main.cpp` được viết **đầy đủ
logic** (đọc khoảng cách gần nhất, phân ngưỡng WARNING/DANGER, beep
non-blocking bằng `millis()`), biến `s_buzzerTaskHandle` cũng đã khai báo sẵn
— nhưng `setup()` **chỉ gọi `xTaskCreatePinnedToCore()` cho 3 task**
(`sensorTask`, `appTask`, `networkTask`), **thiếu hẳn lời gọi cho
`buzzerTask`**. Task này chưa bao giờ được đưa vào scheduler, nên còi vật lý
**chưa từng kêu một lần nào** kể từ khi tính năng được viết — kể cả trước khi
đổi `BUZZER_PIN` từ 48 sang 11.

Đây là loại lỗi trình biên dịch không bắt được: hàm `static` không được gọi
lẽ ra sẽ sinh cảnh báo `-Wunused-function`, nhưng vì `buzzerTask` có địa chỉ
được "dùng" gián tiếp qua khai báo biến handle nên không kích hoạt cảnh báo,
và mọi tài liệu đều mô tả tính năng như đang hoạt động.

Điều đáng chú ý: `report/README.md` mục 5.1 đã từng ghi chú **"cần xác minh
có được khởi tạo bằng `xTaskCreatePinnedToCore` trong `setup()` hay chưa"** —
nghi ngờ đúng nhưng chưa ai kiểm chứng lại.

**Đã sửa**: thêm `xTaskCreatePinnedToCore(buzzerTask, "BuzzerTask", 2048,
nullptr, 1, &s_buzzerTaskHandle, 0)` vào `setup()` (core 0, cùng
`networkTask` — chỉ toggle GPIO theo `millis()`, không có ràng buộc timing
chặt như `sensorTask` nên không cần chiếm core 1).

---

## ✅ Kết quả kiểm thử (Build Verification)

| Project | Command | Kết quả |
| :--- | :--- | :--- |
| `sensor-node` | `pio run -e yolo_uno` | **SUCCESS** — RAM 14.0% (45744/327680B), Flash 21.1% (706813/3342336B) |
| `waveshare-screen` | `build_and_flash.bat build` | **SUCCESS** — RAM 12.3% (40364/327680B), Flash 28.0% (1154145/4128768B) |

Log serial thực tế đọc qua `pio device monitor -p COM18 -b 115200` xác nhận `[S5]` luôn `Pulse: 0 us` trước khi đổi pin (bằng chứng trực tiếp cho nguyên nhân GPIO47/48).

---

## 🚀 Hướng dẫn vận hành / Flash board

```cmd
:: sensor-node — đấu lại dây REAR: Trig -> GPIO 3, Echo -> GPIO 4 (không còn dùng 47/48)
cd firmware/sensor-node
pio run -e yolo_uno -t upload --upload-port COM18
pio device monitor -p COM18 -b 115200

:: waveshare-screen — nhận đúng mapping slot mới (REAR hiện ở "S2 (Rear)", không phải "S6")
cd firmware/waveshare-screen
build_and_flash.bat flash COM9
build_and_flash.bat monitor COM9
```

## 📌 Ghi chú cho lần thêm cảm biến/đổi chân sau này

- **Không dùng GPIO 47/48** trên board `yolo_uno` (ESP32-S3 Embedded PSRAM 8MB) cho bất kỳ tín hiệu số nào — luôn bị chiếm dụng nội bộ cho PSRAM diff-clock.
- **Không dùng GPIO 26-32** (SPI0 flash/PSRAM) và **GPIO 19/20** (USB D-/D+, cần cho Serial CDC).
- Khi thêm/đổi `SENSOR_PINS[]` trong `Config.h`, luôn cập nhật đồng thời `SENSOR_ESPNOW_SLOT[]` trong `EspNowConfig.h` theo **đúng thứ tự vật lý** — hai mảng lệch nhau sẽ không gây lỗi build, chỉ gây sai lệch dữ liệu hiển thị (rất khó phát hiện nếu không đọc kỹ log).
- Log cảnh báo GPIO (`warnIfReservedPin` trong `main.cpp`) sẽ tự động bắt các trường hợp trùng GPIO đã biết ngay lúc boot — kiểm tra Serial Monitor sau mỗi lần đổi pin.
