# Architecture Decision Records (ADR)

> Các quyết định kiến trúc quan trọng của dự án, ghi lại theo định dạng ngắn gọn (lightweight MADR). Khác với [`report/README.md` mục 9](../../report/README.md#9-nhật-ký--lịch-sử-phát-triển) (nhật ký văn xuôi, kể lại **quá trình** đưa ra quyết định kèm sự cố/debug), mỗi ADR ở đây chỉ trích xuất **kết luận** — bối cảnh, quyết định, hệ quả — để tra cứu nhanh mà không cần đọc lại toàn bộ lịch sử phát triển.

## ADR là gì, và tại sao dùng ở đây

ADR (Architecture Decision Record) là một file ngắn ghi lại **một** quyết định kiến trúc: vấn đề gặp phải, các lựa chọn đã cân nhắc, quyết định cuối cùng và đánh đổi (tradeoff) đi kèm. Dự án này đã có sẵn các quyết định lớn ghi rải rác dạng văn xuôi trong `report/README.md` mục 4 và mục 9 — các file trong `docs/adr/` chỉ trích xuất lại chúng thành định dạng có cấu trúc, dễ tra cứu theo từng chủ đề riêng lẻ.

## Định dạng

Mỗi ADR dùng template rút gọn (lightweight MADR):

```markdown
# NNNN. <Tiêu đề>

**Trạng thái**: Accepted
**Ngày**: <ngày nếu xác định được từ git log/docs, nếu không thì ghi giai đoạn ước lượng>

## Bối cảnh (Context)
## Quyết định (Decision)
## Hệ quả (Consequences)
## Tham khảo
```

Trạng thái có thể là `Proposed` / `Accepted` / `Superseded by NNNN` / `Deprecated` — hiện tất cả ADR trong dự án đều ở trạng thái `Accepted` (chưa có quyết định nào bị thay thế).

## Quy ước đặt tên & đánh số

`NNNN-kebab-title.md` — số thứ tự 4 chữ số tăng dần theo thời điểm quyết định được đưa ra (không phải theo mức độ quan trọng), tiêu đề dạng kebab-case tiếng Anh ngắn gọn. Số **không** được tái sử dụng, kể cả khi một ADR bị supersede — thêm ADR mới trỏ ngược lại ADR cũ.

## Danh sách ADR hiện có

| # | Tiêu đề | Trạng thái |
| --- | --- | --- |
| [0001](0001-platformio-unified-build-system.md) | PlatformIO thống nhất cho cả 2 firmware dù khác framework | Accepted |
| [0002](0002-espidf-pure-for-waveshare-screen.md) | `waveshare-screen` dùng ESP-IDF thuần thay vì Arduino/hybrid | Accepted |
| [0003](0003-coreiot-cloud-platform.md) | CoreIoT (ThingsBoard) làm nền tảng cloud IoT | Accepted |
| [0004](0004-cluster-ema-distance-filter.md) | Bộ lọc khoảng cách Cluster + EMA thay vì median filter đơn giản | Accepted |

## Khi nào nên thêm ADR mới

Khi thay đổi một quyết định **có ảnh hưởng lâu dài, khó đảo ngược, hoặc từng cân nhắc nhiều phương án** (đổi framework, đổi nền tảng cloud, đổi thuật toán lọc, đổi giao thức truyền dữ liệu...) — không cần ADR cho các thay đổi cấu hình nhỏ (đã có hướng dẫn ở [`docs/API_GUIDE.md` mục 3](../API_GUIDE.md#3-cấu-hình-bằng-phần-mềm--không-cần-sửa-code-logic)). Xem thêm quy tắc ghi log triển khai (dev diary, khác với ADR) tại [`CONTRIBUTING.md`](../../CONTRIBUTING.md#ghi-log-triển-khai-implementation-logging).
