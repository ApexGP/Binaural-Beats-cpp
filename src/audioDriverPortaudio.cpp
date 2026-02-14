#include "binaural/audioDriver.hpp"
#include <portaudio.h>
#include <atomic>
#include <mutex>
#include <stdexcept>

namespace binaural {

namespace {

struct StreamUserData {
    AudioCallback callback;
    std::vector<int16_t> buffer;
    std::mutex mutex;
    std::atomic<bool> running{false};
};

int portAudioCallback(const void* /*input*/, void* output, unsigned long frameCount,
                      const PaStreamCallbackTimeInfo* /*timeInfo*/,
                      PaStreamCallbackFlags /*flags*/, void* userData) {
    auto* ud = static_cast<StreamUserData*>(userData);
    if (!ud->running) return paComplete;

    ud->buffer.resize(frameCount * 2);
    ud->callback(ud->buffer);

    auto* out = static_cast<int16_t*>(output);
    for (size_t i = 0; i < ud->buffer.size(); ++i) {
        out[i] = ud->buffer[i];
    }
    return paContinue;
}

}  // namespace

class PortAudioDriver : public IAudioDriver {
public:
    PortAudioDriver() {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            throw std::runtime_error(std::string("PortAudio init: ") + Pa_GetErrorText(err));
        }
    }

    ~PortAudioDriver() override {
        stop();
        Pa_Terminate();
    }

    bool start(int sampleRate, int bufferFrames, AudioCallback cb) override {
        if (running_) return false;

        userData_.callback = std::move(cb);
        userData_.buffer.resize(bufferFrames * 2);
        userData_.running = true;

        PaStreamParameters outParam{};
        outParam.device = Pa_GetDefaultOutputDevice();
        if (outParam.device == paNoDevice) {
            userData_.running = false;
            return false;
        }
        outParam.channelCount = 2;
        outParam.sampleFormat = paInt16;
        outParam.suggestedLatency = Pa_GetDeviceInfo(outParam.device)->defaultLowOutputLatency;
        outParam.hostApiSpecificStreamInfo = nullptr;

        PaError err = Pa_OpenStream(&stream_, nullptr, &outParam, sampleRate,
                                   bufferFrames, paClipOff, portAudioCallback, &userData_);
        if (err != paNoError) {
            userData_.running = false;
            return false;
        }

        err = Pa_StartStream(stream_);
        if (err != paNoError) {
            Pa_CloseStream(stream_);
            stream_ = nullptr;
            userData_.running = false;
            return false;
        }

        running_ = true;
        return true;
    }

    void stop() override {
        if (!running_) return;
        userData_.running = false;
        if (stream_) {
            Pa_StopStream(stream_);
            Pa_CloseStream(stream_);
            stream_ = nullptr;
        }
        running_ = false;
    }

    bool isRunning() const override { return running_; }

private:
    PaStream* stream_ = nullptr;
    StreamUserData userData_;
    std::atomic<bool> running_{false};
};

std::unique_ptr<IAudioDriver> createPortAudioDriver() {
    return std::make_unique<PortAudioDriver>();
}

}  // namespace binaural
