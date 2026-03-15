#include "binaural/waveformBuffer.hpp"

namespace binaural {

WaveformBuffer::WaveformBuffer(size_t capacity) : capacity_(capacity) {
    bufL_.resize(capacity, 0.f);
    bufR_.resize(capacity, 0.f);
}

void WaveformBuffer::push(float sampleL, float sampleR) {
    size_t w = writeIdx_.load(std::memory_order_relaxed);
    bufL_[w] = sampleL;
    bufR_[w] = sampleR;
    writeIdx_.store((w + 1) % capacity_, std::memory_order_release);
}

void WaveformBuffer::getSamples(std::vector<float>& outL,
                               std::vector<float>& outR) const {
    size_t w = writeIdx_.load(std::memory_order_acquire);
    outL.resize(capacity_);
    outR.resize(capacity_);
    for (size_t i = 0; i < capacity_; ++i) {
        size_t idx = (w + i) % capacity_;
        outL[i] = bufL_[idx];
        outR[i] = bufR_[idx];
    }
}

}  // namespace binaural
