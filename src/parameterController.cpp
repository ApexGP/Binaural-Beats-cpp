#include "binaural/parameterController.hpp"
#include "binaural/period.hpp"
#include <algorithm>
#include <cmath>

namespace binaural {

namespace {
constexpr float BEAT_FREQ_MIN = 0.f;
constexpr float BEAT_FREQ_MAX = 40.f;
} // namespace

ParameterController::ParameterController(Synthesizer &synth,
                                         PredictionQueue &queue)
    : synth_(&synth), queue_(&queue) {
  // 缓存 dt，bufferFrames/sampleRate 在运行期不变
  const auto &cfg = synth.config();
  dt_ = static_cast<float>(cfg.bufferFrames) / static_cast<float>(cfg.sampleRate);
}

void ParameterController::update(float periodElapsedSec) {
  if (clearRequested_.exchange(false, std::memory_order_acq_rel)) {
    lastPrediction_.reset();
    currentTargetHz_.store(0.f, std::memory_order_relaxed);
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
    if (currentTargetHz_.load(std::memory_order_relaxed) <= 0.f && synth_->currentPeriod() &&
        !synth_->currentPeriod()->voices.empty()) {
      currentTargetHz_.store(synth_->currentPeriod()->voices[0].freqStart,
                             std::memory_order_relaxed);
    }
    float cur = currentTargetHz_.load(std::memory_order_relaxed);
    float diff = target - cur;
    float maxStep = rampRate_ * dt_;  // 使用预缓存的 dt_
    float next;
    if (std::abs(diff) <= maxStep) {
      next = target;
    } else {
      next = cur + (diff > 0 ? maxStep : -maxStep);
    }
    next = std::clamp(next, BEAT_FREQ_MIN, BEAT_FREQ_MAX);
    currentTargetHz_.store(next, std::memory_order_relaxed);

    if (const Period *period = synth_->currentPeriod()) {
      size_t n = period->voices.size();
      // 复用 freqsBuf_，消除每 buffer 的 vector 堆分配
      freqsBuf_.assign(n, currentTargetHz_.load(std::memory_order_relaxed));
      synth_->setFreqs(freqsBuf_);
    }
  } else {
    aiDriven_.store(false, std::memory_order_release);
    lastPrediction_.reset();
    currentTargetHz_.store(0.f, std::memory_order_relaxed);
    synth_->skewVoices(periodElapsedSec);
  }
}

} // namespace binaural
