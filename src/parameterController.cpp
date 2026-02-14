#include "binaural/parameterController.hpp"
#include "binaural/period.hpp"
#include <algorithm>
#include <cmath>

namespace binaural {

namespace {
constexpr float BEAT_FREQ_MIN = 0.5f;
constexpr float BEAT_FREQ_MAX = 40.f;
} // namespace

ParameterController::ParameterController(Synthesizer &synth,
                                         PredictionQueue &queue)
    : synth_(&synth), queue_(&queue) {}

void ParameterController::update(float periodElapsedSec) {
  if (clearRequested_.exchange(false, std::memory_order_acq_rel)) {
    lastPrediction_.reset();
    currentTargetHz_ = 0.f;
    aiDriven_.store(false, std::memory_order_release);
    synth_->skewVoices(periodElapsedSec);
    return;
  }

  auto pred = queue_->popLatest();
  if (pred && pred->confidence > 0.01f) {
    lastPrediction_ = *pred;
  }

  if (lastPrediction_ && lastPrediction_->confidence > 0.01f) {
    aiDriven_.store(true, std::memory_order_release);
    float target = std::clamp(lastPrediction_->targetBeatFreq, BEAT_FREQ_MIN,
                             BEAT_FREQ_MAX);
    if (currentTargetHz_ <= 0.f && synth_->currentPeriod()) {
      currentTargetHz_ = synth_->currentPeriod()->voices[0].freqStart;
    }
    const auto &cfg = synth_->config();
    float dt = static_cast<float>(cfg.bufferFrames) / cfg.sampleRate;
    float diff = target - currentTargetHz_;
    float maxStep = rampRate_ * dt;
    if (std::abs(diff) <= maxStep) {
      currentTargetHz_ = target;
    } else {
      currentTargetHz_ += (diff > 0 ? maxStep : -maxStep);
    }
    currentTargetHz_ =
        std::clamp(currentTargetHz_, BEAT_FREQ_MIN, BEAT_FREQ_MAX);

    if (const Period *period = synth_->currentPeriod()) {
      size_t n = period->voices.size();
      std::vector<float> freqs(n, currentTargetHz_);
      synth_->setFreqs(freqs);
    }
  } else {
    aiDriven_.store(false, std::memory_order_release);
    lastPrediction_.reset();
    currentTargetHz_ = 0.f;
    synth_->skewVoices(periodElapsedSec);
  }
}

} // namespace binaural
