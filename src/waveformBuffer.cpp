#include "binaural/waveformBuffer.hpp"

namespace binaural {

WaveformBuffer::WaveformBuffer(size_t capacity) : capacity_(capacity) {
    bufL_.resize(capacity, 0.f);
    bufR_.resize(capacity, 0.f);
}

void WaveformBuffer::push(float sampleL, float sampleR) {
    std::lock_guard lock(mutex_);
    bufL_[writeIdx_] = sampleL;
    bufR_[writeIdx_] = sampleR;
    writeIdx_ = (writeIdx_ + 1) % capacity_;
}

void WaveformBuffer::getSamples(std::vector<float>& outL,
                               std::vector<float>& outR) const {
    std::lock_guard lock(mutex_);
    outL.resize(capacity_);
    outR.resize(capacity_);
    for (size_t i = 0; i < capacity_; ++i) {
        size_t idx = (writeIdx_ + i) % capacity_;
        outL[i] = bufL_[idx];
        outR[i] = bufR_[idx];
    }
}

}  // namespace binaural
