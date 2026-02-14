#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace binaural {

/// 音频驱动抽象：请求填充 bufferFrames 帧立体声样本
using AudioCallback = std::function<void(std::vector<int16_t>&)>;

class IAudioDriver {
public:
    virtual ~IAudioDriver() = default;
    virtual bool start(int sampleRate, int bufferFrames, AudioCallback cb) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

std::unique_ptr<IAudioDriver> createPortAudioDriver();

}  // namespace binaural
