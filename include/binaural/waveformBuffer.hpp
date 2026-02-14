#pragma once

#include <atomic>
#include <mutex>
#include <vector>

namespace binaural {

/// 环形缓冲，供音频回调写入、UI 线程读取用于波形显示
class WaveformBuffer {
public:
    explicit WaveformBuffer(size_t capacity = 4096);

    void push(float sampleL, float sampleR);
    void getSamples(std::vector<float>& outL, std::vector<float>& outR) const;

private:
    mutable std::mutex mutex_;
    std::vector<float> bufL_;
    std::vector<float> bufR_;
    size_t writeIdx_ = 0;
    size_t capacity_;
};

}  // namespace binaural
