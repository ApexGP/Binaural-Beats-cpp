#pragma once

#include "binaural/audioDriver.hpp"
#include "binaural/parameterController.hpp"
#include "binaural/period.hpp"
#include "binaural/synthesizer.hpp"
#include "binaural/waveformBuffer.hpp"

namespace binaural {
struct Program;
}

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
  float manualElapsedSec = 0.f;
  char loadPathBuf[512] = {};
  bool timedPlaybackEnabled = false;
  float timedPlaybackDurationSec = 600.f;
  bool showTimedPlaybackModal = false;
  bool modalOpen = false;

  // DPI scaling (set each frame in mainLoop)
  float uiScale = 1.f;
};

void renderTitleBar(AppContext &ctx);
void renderWaveform(AppContext &ctx);
void renderBeatDescription(AppContext &ctx);
void renderControls(AppContext &ctx);
void renderHelpCenter(AppContext &ctx);
void renderLoadModal(AppContext &ctx);
void renderTimedPlaybackModal(AppContext &ctx);

} // namespace gui
