#pragma once
#include <stdint.h>
#include <stddef.h>
#include "Config.h"

// Kết quả một lần xử lý bộ lọc, tương ứng với filter_distance() trong bản Python
struct FilterResult
{
    bool hasCluster; // có tìm được cụm đủ mạnh không
    float clusterCm;
    int clusterCount; // số phiếu (mẫu) của cụm, trong tổng raw_history

    bool hasSpread;
    float clusterSpreadCm;

    bool hasOutput; // đã có kết quả ổn định để dùng chưa
    float outputCm;

    const char *status; // "WARMUP" | "NO_CLUSTER" | "INIT" | "OK" | "HOLD_JUMP" | "ACCEPT_JUMP"
};

// Bộ lọc median/cluster cho MỘT cảm biến. Mỗi cảm biến trong mảng cần
// một instance riêng (state không dùng chung). Lịch sử mẫu dùng mảng
// tĩnh kích thước HISTORY_SIZE thay cho std::vector để tránh cấp phát
// động trên heap và tăng tốc xử lý trên vi điều khiển.
class DistanceFilter
{
public:
    // Xóa toàn bộ trạng thái bộ lọc (lịch sử, kết quả ổn định, ứng viên bước nhảy)
    void reset();

    // Xử lý một mẫu RAW hợp lệ (đã qua kiểm tra phạm vi ở tầng cảm biến)
    FilterResult process(float rawDistanceCm);

    // Lấy khoảng cách ổn định gần nhất, trả về false nếu chưa có
    bool getStable(float &outCm) const;

private:
    float _rawHistory[HISTORY_SIZE];
    size_t _historyCount = 0;

    bool _hasStable = false;
    float _stableDistanceCm = 0.0f;

    bool _hasJumpCandidate = false;
    float _jumpCandidateCm = 0.0f;
    int _jumpCandidateCount = 0;

    void addToHistory(float distanceCm);
    void replaceHistoryWith(const float *values, size_t count);
    void clearJumpCandidate();
    bool updateJumpCandidate(float newDistanceCm);
};
