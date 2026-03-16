#pragma once

#include <atomic>
#include <vector>

namespace binaural {

/// 无锁 SPSC 环形缓冲：音频线程写，GUI 线程读（波形显示）
/// 写者只写 writeIdx_，读者只读 writeIdx_（acquire）
class WaveformBuffer
{
public:
    explicit WaveformBuffer(size_t capacity = 4096);

    /// 音频线程调用
    void push(float sampleL, float sampleR);
    /// GUI 线程调用
    void getSamples(std::vector<float>& outL, std::vector<float>& outR) const;

private:
    std::vector<float> bufL_;
    std::vector<float> bufR_;
    std::atomic<size_t> writeIdx_{0};
    size_t capacity_;
};

}  // namespace binaural
