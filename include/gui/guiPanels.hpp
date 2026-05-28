#pragma once

#include <atomic>

#include "binaural/audioDriver.hpp"
#include "binaural/parameterController.hpp"
#include "binaural/period.hpp"
#include "binaural/synthesizer.hpp"
#include "binaural/waveformBuffer.hpp"

namespace gui {

struct AppContext {
    binaural::Program &program;
    binaural::Synthesizer &synth;
    binaural::ParameterController &paramController;
    binaural::ParameterController::PredictionQueue &predQueue;
    binaural::WaveformBuffer &waveBuf;
    const binaural::SynthesizerConfig &config;
    binaural::IAudioDriver *driver;

    float beatFreq = 4.f;
    float baseFreq = 161.f;
    float balance = 0.f;
    float volume = 0.7f;
    bool playing = false;
    bool showLoadModal = false;
    bool showHelpCenter = false;
    bool loadedFromGnaural = false;
    std::atomic<float>& manualElapsedSec;  // 音频线程写，GUI 线程读
    char (&loadPathBuf)[512];
    bool timedPlaybackEnabled = false;
    float timedPlaybackDurationSec = 600.f;
    bool showTimedPlaybackModal = false;
    bool showMenu = false;
    bool modalOpen = false;

    float uiScale = 1.f;
};

bool isBackgroundDropdownOpen();

}  // namespace gui
