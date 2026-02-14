#pragma once

#include "eegPredictorInterface.hpp"
#include <optional>
#include <vector>

namespace binaural {

/// 无 BCI 时的默认预测器：返回 nullopt 或可配置的固定参数（用于测试）
class StubPredictor : public IEEGStatePredictor {
public:
    /// 若启用 simulate，则返回固定预测（用于验证闭环管线）
    explicit StubPredictor(bool simulate = false, float fixedTargetHz = 10.f);

    std::optional<EEGStatePrediction> predict(
        const std::vector<std::vector<float>>& eegChannels,
        float sampleRateHz) override;

    bool isRealtimeCapable() const override { return true; }

    void setSimulate(bool v) { simulate_ = v; }
    void setFixedTargetHz(float hz) { fixedTargetHz_ = hz; }

private:
    bool simulate_ = false;
    float fixedTargetHz_ = 10.f;
};

}  // namespace binaural
