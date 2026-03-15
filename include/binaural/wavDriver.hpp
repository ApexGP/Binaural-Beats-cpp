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

    /// 流式写入 WAV，写完自动填充 header 中的 size 字段
    /// 返回 false 表示文件打开或写入失败
    bool writeToFile(const std::string& path, float durationSec);

private:
    int sampleRate_ = 44100;
    int bufferFrames_ = 2048;
    AudioCallback callback_;
    std::atomic<bool> running_{false};
};

}  // namespace binaural
