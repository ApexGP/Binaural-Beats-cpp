#include "binaural/audioDriver.hpp"
#include "binaural/period.hpp"
#include "binaural/synthesizer.hpp"
#include "binaural/wavDriver.hpp"
#include <iostream>

using namespace binaural;

int main() {
    Program program;
    program.name = "Alpha Relax";
    program.seq.push_back({
        .lengthSec = 60,
        .voices = {{
            .freqStart = 10.0f,
            .freqEnd = 10.0f,
            .volume = 0.7f,
            .pitch = -1.f,
            .isochronic = false,
        }},
        .background = Period::Background::None,
        .backgroundVol = 0.f,
    });

    SynthesizerConfig config;
    config.sampleRate = 44100;
    config.bufferFrames = 2048;
    config.iscale = 1440;

    Synthesizer synth(config);
    synth.setProgram(program);

    auto driver = createPortAudioDriver();
    bool ok = driver->start(config.sampleRate, config.bufferFrames,
        [&synth, &config](std::vector<int16_t>& buf) {
            synth.skewVoices(synth.periodElapsedSec());
            synth.fillSamples(buf);
            const float frameSec =
                static_cast<float>(config.bufferFrames) / config.sampleRate;
            synth.advanceTime(frameSec);
        });

    if (!ok) {
        std::cerr << "Failed to start audio. Check PortAudio/device.\n";
        return 1;
    }

    if (auto* wav = dynamic_cast<WavFileDriver*>(driver.get())) {
        std::cout << "PortAudio not found. Writing output.wav (10 sec)...\n";
        wav->writeToFile("output.wav", 10.0f);
        std::cout << "Done. Play output.wav to verify.\n";
        return 0;
    }

    std::cout << "Playing: " << program.name
              << " (10 Hz Alpha). Press Enter to stop.\n";
    std::cin.get();

    driver->stop();
    std::cout << "Stopped.\n";
    return 0;
}
