#include "DistanceFilter.h"
#include <math.h>
#include <string.h>

// =========================================================
// HÀM HỖ TRỢ - KHÔNG DÙNG STL
// Mảng tĩnh kích thước HISTORY_SIZE, sort/min/max viết tay
// thay cho std::vector/std::sort/std::max_element.
// =========================================================

namespace
{

// Insertion sort tại chỗ - đủ nhanh vì count <= HISTORY_SIZE (rất nhỏ).
float sortedMedian(float *values, size_t count)
{
    for (size_t i = 1; i < count; ++i)
    {
        float key = values[i];
        size_t j = i;
        while (j > 0 && values[j - 1] > key)
        {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = key;
    }

    size_t middle = count / 2;
    if (count % 2 == 1)
    {
        return values[middle];
    }
    return (values[middle - 1] + values[middle]) / 2.0f;
}

float getClusterTolerance(float centerCm)
{
    float dynamicTolerance = centerCm * CLUSTER_TOLERANCE_RATIO;
    return dynamicTolerance < BASE_CLUSTER_TOLERANCE_CM
               ? BASE_CLUSTER_TOLERANCE_CM
               : dynamicTolerance;
}

float getJumpThreshold(float currentDistanceCm)
{
    float dynamicThreshold = currentDistanceCm * JUMP_THRESHOLD_RATIO;
    return dynamicThreshold < MIN_JUMP_THRESHOLD_CM
               ? MIN_JUMP_THRESHOLD_CM
               : dynamicThreshold;
}

float getJumpTolerance(float candidateCm)
{
    float dynamicTolerance = candidateCm * JUMP_TOLERANCE_RATIO;
    return dynamicTolerance < BASE_JUMP_TOLERANCE_CM
               ? BASE_JUMP_TOLERANCE_CM
               : dynamicTolerance;
}

struct ClusterSearchResult
{
    bool found;
    float center;
    float members[HISTORY_SIZE];
    size_t memberCount;
    float spread;
};

// Tương đương find_best_cluster() bản Python, nhưng toàn bộ mảng trung
// gian là mảng tĩnh HISTORY_SIZE phần tử (history luôn <= HISTORY_SIZE
// nên members/bestMembers không bao giờ tràn).
ClusterSearchResult findBestCluster(const float *values, size_t count,
                                     bool hasReference, float referenceCm)
{
    ClusterSearchResult best;
    best.found = false;
    best.center = 0.0f;
    best.memberCount = 0;
    best.spread = 0.0f;

    if (count < (size_t)MIN_SAMPLES_TO_FILTER)
    {
        return best;
    }

    float bestMembers[HISTORY_SIZE];
    size_t bestMemberCount = 0;
    float bestCenter = 0.0f;
    bool hasBestSpread = false;
    float bestSpread = 0.0f;
    bool hasBestReferenceDifference = false;
    float bestReferenceDifference = 0.0f;

    for (size_t c = 0; c < count; ++c)
    {
        float candidateCenter = values[c];
        float toleranceCm = getClusterTolerance(candidateCenter);

        float members[HISTORY_SIZE];
        size_t memberCount = 0;
        for (size_t v = 0; v < count; ++v)
        {
            if (fabsf(values[v] - candidateCenter) <= toleranceCm)
            {
                members[memberCount++] = values[v];
            }
        }

        if (memberCount == 0)
        {
            continue;
        }

        float sortedMembers[HISTORY_SIZE];
        memcpy(sortedMembers, members, memberCount * sizeof(float));
        float centerCm = sortedMedian(sortedMembers, memberCount);

        float minCm = members[0];
        float maxCm = members[0];
        for (size_t v = 1; v < memberCount; ++v)
        {
            if (members[v] < minCm) minCm = members[v];
            if (members[v] > maxCm) maxCm = members[v];
        }
        float spreadCm = maxCm - minCm;

        float referenceDifference = 0.0f;
        if (hasReference)
        {
            referenceDifference = fabsf(centerCm - referenceCm);
        }

        bool replaceBest = false;

        // Ưu tiên cụm có nhiều phiếu hơn
        if (memberCount > bestMemberCount)
        {
            replaceBest = true;
        }
        // Nếu bằng số phiếu, chọn cụm chặt hơn
        else if (memberCount == bestMemberCount)
        {
            if (!hasBestSpread)
            {
                replaceBest = true;
            }
            else if (spreadCm < bestSpread)
            {
                replaceBest = true;
            }
            // Nếu độ phân tán bằng nhau, chọn cụm gần kết quả cũ hơn
            else if (fabsf(spreadCm - bestSpread) < 0.001f &&
                     hasBestReferenceDifference &&
                     referenceDifference < bestReferenceDifference)
            {
                replaceBest = true;
            }
        }

        if (replaceBest)
        {
            memcpy(bestMembers, members, memberCount * sizeof(float));
            bestMemberCount = memberCount;
            bestCenter = centerCm;
            hasBestSpread = true;
            bestSpread = spreadCm;
            hasBestReferenceDifference = true;
            bestReferenceDifference = referenceDifference;
        }
    }

    memcpy(best.members, bestMembers, bestMemberCount * sizeof(float));
    best.memberCount = bestMemberCount;
    best.spread = bestSpread;

    if (bestMemberCount < (size_t)MIN_CLUSTER_SIZE)
    {
        best.found = false;
        return best;
    }

    best.found = true;
    best.center = bestCenter;
    return best;
}

} // namespace

// =========================================================
// DistanceFilter
// =========================================================

void DistanceFilter::clearJumpCandidate()
{
    _hasJumpCandidate = false;
    _jumpCandidateCm = 0.0f;
    _jumpCandidateCount = 0;
}

void DistanceFilter::reset()
{
    _historyCount = 0;
    _hasStable = false;
    _stableDistanceCm = 0.0f;
    clearJumpCandidate();
}

// Thay cho std::vector::push_back + erase(begin()): khi đầy, dịch trái
// 1 ô để bỏ mẫu cũ nhất rồi thêm mẫu mới vào cuối.
void DistanceFilter::addToHistory(float distanceCm)
{
    if (_historyCount < HISTORY_SIZE)
    {
        _rawHistory[_historyCount++] = distanceCm;
        return;
    }

    for (size_t i = 1; i < HISTORY_SIZE; ++i)
    {
        _rawHistory[i - 1] = _rawHistory[i];
    }
    _rawHistory[HISTORY_SIZE - 1] = distanceCm;
}

// Thay cho "s_rawHistory = cluster.members" - values luôn là tập con của
// _rawHistory nên count <= HISTORY_SIZE, không cần cắt bớt.
void DistanceFilter::replaceHistoryWith(const float *values, size_t count)
{
    size_t copyCount = count > HISTORY_SIZE ? HISTORY_SIZE : count;
    size_t skip = count - copyCount;
    for (size_t i = 0; i < copyCount; ++i)
    {
        _rawHistory[i] = values[skip + i];
    }
    _historyCount = copyCount;
}

bool DistanceFilter::updateJumpCandidate(float newDistanceCm)
{
    if (!_hasJumpCandidate)
    {
        _hasJumpCandidate = true;
        _jumpCandidateCm = newDistanceCm;
        _jumpCandidateCount = 1;
        return false;
    }

    float toleranceCm = getJumpTolerance(_jumpCandidateCm);
    float differenceCm = fabsf(newDistanceCm - _jumpCandidateCm);

    if (differenceCm <= toleranceCm)
    {
        _jumpCandidateCount++;
        _jumpCandidateCm = (_jumpCandidateCm + newDistanceCm) / 2.0f;
    }
    else
    {
        _jumpCandidateCm = newDistanceCm;
        _jumpCandidateCount = 1;
    }

    return _jumpCandidateCount >= JUMP_CONFIRM_COUNT;
}

FilterResult DistanceFilter::process(float rawDistanceCm)
{
    FilterResult result;
    result.hasCluster = false;
    result.clusterCm = 0.0f;
    result.clusterCount = 0;
    result.hasSpread = false;
    result.clusterSpreadCm = 0.0f;
    result.hasOutput = _hasStable;
    result.outputCm = _stableDistanceCm;
    result.status = "WARMUP";

    addToHistory(rawDistanceCm);

    // Chưa đủ mẫu
    if (_historyCount < (size_t)MIN_SAMPLES_TO_FILTER)
    {
        return result;
    }

    ClusterSearchResult cluster =
        findBestCluster(_rawHistory, _historyCount, _hasStable, _stableDistanceCm);

    result.clusterCount = (int)cluster.memberCount;

    if (cluster.memberCount > 0)
    {
        result.hasSpread = true;
        result.clusterSpreadCm = cluster.spread;
    }

    // Không có cụm đủ mạnh
    if (!cluster.found)
    {
        clearJumpCandidate();
        result.status = "NO_CLUSTER";
        return result;
    }

    result.hasCluster = true;
    result.clusterCm = cluster.center;

    // Khởi tạo kết quả đầu tiên
    if (!_hasStable)
    {
        _hasStable = true;
        _stableDistanceCm = cluster.center;
        clearJumpCandidate();

        result.hasOutput = true;
        result.outputCm = _stableDistanceCm;
        result.status = "INIT";
        return result;
    }

    float differenceCm = fabsf(cluster.center - _stableDistanceCm);
    float jumpThresholdCm = getJumpThreshold(_stableDistanceCm);

    // Cụm mới vẫn gần kết quả hiện tại
    if (differenceCm <= jumpThresholdCm)
    {
        clearJumpCandidate();

        _stableDistanceCm =
            EMA_ALPHA * cluster.center + (1.0f - EMA_ALPHA) * _stableDistanceCm;

        result.hasOutput = true;
        result.outputCm = _stableDistanceCm;
        result.status = "OK";
        return result;
    }

    // Cụm mới cách xa kết quả hiện tại -> cần xác nhận bước nhảy
    bool jumpConfirmed = updateJumpCandidate(cluster.center);

    if (!jumpConfirmed)
    {
        result.hasOutput = _hasStable;
        result.outputCm = _stableDistanceCm;
        result.status = "HOLD_JUMP";
        return result;
    }

    // Đã xác nhận vị trí mới
    _stableDistanceCm = _jumpCandidateCm;

    // Bỏ các mẫu cũ, chỉ giữ cụm mới
    replaceHistoryWith(cluster.members, cluster.memberCount);

    clearJumpCandidate();

    result.hasOutput = true;
    result.outputCm = _stableDistanceCm;
    result.status = "ACCEPT_JUMP";
    return result;
}

bool DistanceFilter::getStable(float &outCm) const
{
    if (!_hasStable)
    {
        return false;
    }
    outCm = _stableDistanceCm;
    return true;
}
