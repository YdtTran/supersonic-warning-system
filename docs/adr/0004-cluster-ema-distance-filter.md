# 0004. Bộ lọc khoảng cách Cluster + EMA thay vì median filter đơn giản

**Trạng thái**: Accepted
**Ngày**: giai đoạn ~2026-08-03 (viết `DistanceFilter`, thay thế bộ lọc Kalman/Median-5 dùng ở prototype `water-level-uart`)

## Bối cảnh (Context)

Cảm biến siêu âm giá rẻ JSN-SR04T dễ sinh ra outlier do phản xạ đa hướng (multi-path reflection) — một số mẫu đo trả về giá trị bất thường, xa hẳn khoảng cách thật, xen kẽ với các mẫu hợp lệ. Bộ lọc cần: (1) loại bỏ outlier hiệu quả, (2) làm mượt kết quả hiển thị để không "giật" trên UI/còi báo, nhưng (3) vẫn phản ứng đủ nhanh khi có thay đổi khoảng cách **thật** (vật cản mới xuất hiện/biến mất) — không được trễ tới mức vô dụng cho ứng dụng cảnh báo va chạm cần phản hồi gần thời gian thực. Prototype đo mực nước (`water-level-uart`) ban đầu dùng bộ lọc Kalman, sau đó được thay bằng chính thuật toán Cluster+EMA của `sensor-node` để đồng bộ cách xử lý giữa 2 project.

## Quyết định (Decision)

Dùng thuật toán 2 tầng trong `DistanceFilter` ([`firmware/sensor-node/include/DistanceFilter.h`](../../firmware/sensor-node/include/DistanceFilter.h)):

1. **Phân cụm (clustering)**: giữ lại `HISTORY_SIZE=9` mẫu RAW gần nhất, nhóm các mẫu gần nhau (trong dung sai `BASE_CLUSTER_TOLERANCE_CM=8cm` + `CLUSTER_TOLERANCE_RATIO=0.08` tăng theo khoảng cách) thành cụm; một cụm chỉ hợp lệ nếu chiếm ít nhất `MIN_CLUSTER_SIZE=5/9` mẫu — loại bỏ outlier rời rạc không tạo thành cụm đủ lớn.
2. **EMA (Exponential Moving Average)**: làm mượt kết quả cụm hợp lệ với hệ số `EMA_ALPHA=0.30`.
3. **Xác nhận bước nhảy (jump confirmation)**: khi kết quả mới chênh lệch lớn (`MIN_JUMP_THRESHOLD_CM=30cm` / `JUMP_THRESHOLD_RATIO=0.25`) so với giá trị ổn định hiện tại, không chấp nhận ngay — chờ xác nhận liên tiếp `JUMP_CONFIRM_COUNT=3` lần trước khi coi là thay đổi thật, tránh nhiễu tức thời bị hiểu nhầm là vật cản mới.
4. Sau `RESET_AFTER_INVALID=15` lần đọc lỗi liên tiếp, bộ lọc tự reset toàn bộ trạng thái và báo mất tín hiệu thay vì tiếp tục xuất giá trị cũ.

Bảng tham số đầy đủ: xem [`report/README.md` mục 5.2](../../report/README.md#52-bộ-lọc-khoảng-cách-cluster--ema).

## Hệ quả (Consequences)

**Tích cực:**
- Loại outlier hiệu quả hơn median filter đơn giản: median chỉ chống được outlier khi chúng là thiểu số trong cửa sổ mẫu, nhưng không phân biệt được "outlier ngẫu nhiên" với "cụm mẫu ổn định mới xuất hiện" — clustering giải quyết trực tiếp vấn đề này bằng cách yêu cầu đồng thuận `MIN_CLUSTER_SIZE` mẫu.
- EMA giữ kết quả hiển thị mượt (không giật số trên UI/không kêu còi liên tục do nhiễu 1 mẫu), trong khi cơ chế "jump" riêng biệt vẫn cho phép phản ứng nhanh với thay đổi thật, không bị EMA làm trễ quá mức.
- Không dùng cấp phát động (`std::vector`) — toàn bộ lịch sử lưu trong mảng tĩnh kích thước cố định (`HISTORY_SIZE=9`), tránh phân mảnh heap trên vi điều khiển, nhất quán với convention "không cấp phát động" của các thư viện `sensor-node` khác.
- Đã được dùng lại thành công ở prototype `water-level-uart` (thay Kalman filter), cho thấy thuật toán đủ tổng quát cho bài toán đo khoảng cách bằng cảm biến siêu âm khác ngoài cảnh báo va chạm.

**Đánh đổi:**
- Nhiều tham số cần tinh chỉnh (`HISTORY_SIZE`, `MIN_CLUSTER_SIZE`, dung sai cụm, `EMA_ALPHA`, ngưỡng jump, `JUMP_CONFIRM_COUNT`) so với median filter chỉ có 1 tham số (kích thước cửa sổ) — độ phức tạp cấu hình cao hơn, dễ chỉnh sai nếu không hiểu rõ ý nghĩa từng tham số (đã có bảng hướng dẫn tinh chỉnh ở [`docs/API_GUIDE.md` mục 3.3](../API_GUIDE.md#33-tinh-chỉnh-bộ-lọc-khoảng-cách-cluster--ema)).
- `MIN_SAMPLES_TO_FILTER=5` nghĩa là cần tối thiểu 5 mẫu mới bắt đầu xuất kết quả — có độ trễ khởi động (warm-up) nhỏ so với median filter có thể xuất kết quả ngay từ mẫu thứ 1 (dù kém tin cậy hơn).
- `JUMP_CONFIRM_COUNT=3` là đánh đổi trực tiếp giữa tốc độ xác nhận vật cản mới và nguy cơ báo giả — đã ghi rõ trong bảng tinh chỉnh để người dùng tương lai hiểu tradeoff khi thay đổi.

## Tham khảo

- [`report/README.md` mục 5.2](../../report/README.md#52-bộ-lọc-khoảng-cách-cluster--ema) — bảng tham số đầy đủ kèm ý nghĩa từng tham số.
- [`docs/API_GUIDE.md` mục 1.2](../API_GUIDE.md#12-distancefilter--lọc-cụm-cluster--ema) — API `DistanceFilter`, ví dụ code.
- [`docs/API_GUIDE.md` mục 3.3](../API_GUIDE.md#33-tinh-chỉnh-bộ-lọc-khoảng-cách-cluster--ema) — hướng dẫn tinh chỉnh tham số theo mục đích (mượt hơn/nhanh hơn/chống nhiễu môi trường).
- Commit [`1dc8740`](https://github.com/YdtTran/supersonic-warning-system/commit/1dc8740) — `water-level-uart` thay Kalman filter bằng thuật toán này.
