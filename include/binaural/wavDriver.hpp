#pragma once

#include "audioDriver.hpp"
#include <atomic>
#include <string>

namespace binaural {

/// 无 PortAudio 时：生成 WAV 文件用于验证算法
class WavFileDriver : public IAudioDriver {
public:
    bool start(int sampleRate, int bufferFrames, AudioCallback cb) override;
    void stop() override;
    bool isRunning() const override;

    void writeToFile(const std::string& path, float durationSec);

private:
    int sampleRate_ = 44100;
    int bufferFrames_ = 2048;
    AudioCallback callback_;
    std::atomic<bool> running_{false};
};

}  // namespace binaural
