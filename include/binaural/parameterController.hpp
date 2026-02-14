#pragma once

#include "eegPredictorInterface.hpp"
#include "lockFreeQueue.hpp"
#include "synthesizer.hpp"
#include <atomic>
#include <optional>

namespace binaural {

/// 参数控制层：轮询预测队列，应用 Ramping 引导策略，更新合成器频率
/// 每 50ms 轮询取最新预测；有预测时用 targetBeatFreq 渐变，无预测时用 program
/// skewVoices
class ParameterController {
public:
  using PredictionQueue = LockFreeQueue<EEGStatePrediction, 8>;

  explicit ParameterController(Synthesizer &synth, PredictionQueue &queue);

  /// 在音频回调中调用（或控制线程每 buffer）：轮询队列，更新 freqs
  void update(float periodElapsedSec);

  /// 是否当前由 AI 预测驱动（用于 UI 显示）
  bool isAiDriven() const { return aiDriven_.load(std::memory_order_acquire); }

  /// 当前有效节拍频率（AI 模式下为 ramping 目标，供 UI 显示）
  float currentBeatFreq() const { return currentTargetHz_; }

  /// Ramping 速率：Hz/秒，默认 2.0
  void setRampRate(float hzPerSec) { rampRate_ = hzPerSec; }

  /// 清除 AI 状态，停止播放或返回手动时调用
  void clearAiState() {
    aiDriven_.store(false, std::memory_order_release);
    clearRequested_.store(true, std::memory_order_release);
  }

private:
  Synthesizer *synth_;
  PredictionQueue *queue_;
  float currentTargetHz_ = 0.f;
  std::atomic<bool> aiDriven_{false};
  std::atomic<bool> clearRequested_{false};
  float rampRate_ = 2.0f;
  std::optional<EEGStatePrediction> lastPrediction_;
};

} // namespace binaural
