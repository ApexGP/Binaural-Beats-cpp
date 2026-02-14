#include "binaural/wavDriver.hpp"
#include <fstream>
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

void WavFileDriver::writeToFile(const std::string& path, float durationSec) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return;

    std::vector<int16_t> buf(bufferFrames_ * 2);
    const int totalFrames = static_cast<int>(durationSec * sampleRate_);
    const int numChunks = (totalFrames + bufferFrames_ - 1) / bufferFrames_;

    std::vector<int16_t> allSamples;
    allSamples.reserve(totalFrames * 2);
    for (int i = 0; i < numChunks; ++i) {
        callback_(buf);
        for (int16_t s : buf) allSamples.push_back(s);
    }

    const int dataSize = static_cast<int>(allSamples.size()) * 2;
    const int fileSize = 36 + dataSize;

    f.write("RIFF", 4);
    f.write(reinterpret_cast<const char*>(&fileSize), 4);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    const int fmtLen = 16;
    f.write(reinterpret_cast<const char*>(&fmtLen), 4);
    const int16_t audioFormat = 1;
    f.write(reinterpret_cast<const char*>(&audioFormat), 2);
    const int16_t numChannels = 2;
    f.write(reinterpret_cast<const char*>(&numChannels), 2);
    f.write(reinterpret_cast<const char*>(&sampleRate_), 4);
    const int byteRate = sampleRate_ * 4;
    f.write(reinterpret_cast<const char*>(&byteRate), 4);
    const int16_t blockAlign = 4;
    f.write(reinterpret_cast<const char*>(&blockAlign), 2);
    const int16_t bitsPerSample = 16;
    f.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    f.write("data", 4);
    f.write(reinterpret_cast<const char*>(&dataSize), 4);
    f.write(reinterpret_cast<const char*>(allSamples.data()), dataSize);
}

}  // namespace binaural
