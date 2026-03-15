#include "binaural/wavDriver.hpp"
#include <cstdio>
#include <vector>

namespace binaural {

bool WavFileDriver::start(int sampleRate, int bufferFrames, AudioCallback cb) {
    if (running_) return false;
    sampleRate_ = sampleRate;
    bufferFrames_ = bufferFrames;
    callback_ = std::move(cb);
    running_ = true;
    return true;
}

void WavFileDriver::stop() { running_ = false; }

bool WavFileDriver::isRunning() const { return running_; }

bool WavFileDriver::writeToFile(const std::string& path, float durationSec) {
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;

    const int totalFrames = static_cast<int>(durationSec * sampleRate_);
    const int numChunks   = (totalFrames + bufferFrames_ - 1) / bufferFrames_;

    // WAV header（44 字节），size 字段先写 0，后填
    // RIFF chunk
    std::fwrite("RIFF", 1, 4, fp);
    uint32_t fileSizePlaceholder = 0;
    std::fwrite(&fileSizePlaceholder, 4, 1, fp);   // offset 4
    std::fwrite("WAVE", 1, 4, fp);
    // fmt sub-chunk
    std::fwrite("fmt ", 1, 4, fp);
    const uint32_t fmtLen   = 16;
    std::fwrite(&fmtLen, 4, 1, fp);
    const uint16_t audioFmt = 1;   // PCM
    std::fwrite(&audioFmt, 2, 1, fp);
    const uint16_t channels = 2;
    std::fwrite(&channels, 2, 1, fp);
    const uint32_t sr = static_cast<uint32_t>(sampleRate_);
    std::fwrite(&sr, 4, 1, fp);
    const uint32_t byteRate = sr * 4u;
    std::fwrite(&byteRate, 4, 1, fp);
    const uint16_t blockAlign   = 4;
    std::fwrite(&blockAlign, 2, 1, fp);
    const uint16_t bitsPerSample = 16;
    std::fwrite(&bitsPerSample, 2, 1, fp);
    // data sub-chunk
    std::fwrite("data", 1, 4, fp);
    uint32_t dataSizePlaceholder = 0;
    std::fwrite(&dataSizePlaceholder, 4, 1, fp);   // offset 40

    // 流式写入 PCM
    std::vector<int16_t> buf(bufferFrames_ * 2);
    uint32_t bytesWritten = 0;
    for (int i = 0; i < numChunks; ++i) {
        callback_(buf);
        const uint32_t chunkBytes = static_cast<uint32_t>(buf.size()) * 2u;
        if (std::fwrite(buf.data(), 2, buf.size(), fp) != buf.size()) {
            std::fclose(fp);
            return false;
        }
        bytesWritten += chunkBytes;
    }

    // 回填 size 字段（L2 修复：uint32_t）
    const uint32_t dataSize = bytesWritten;
    const uint32_t fileSize = 36u + dataSize;

    std::fseek(fp, 4, SEEK_SET);
    std::fwrite(&fileSize, 4, 1, fp);
    std::fseek(fp, 40, SEEK_SET);
    std::fwrite(&dataSize, 4, 1, fp);

    std::fclose(fp);
    return true;
}

}  // namespace binaural
