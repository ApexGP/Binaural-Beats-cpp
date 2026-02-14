#pragma once

#include <optional>
#include <vector>

namespace binaural {

/// EEG 状态预测结果，供闭环控制使用
struct EEGStatePrediction {
    enum class State { Relaxed, Focused, Drowsy, Anxious, Unknown };
    State state = State::Unknown;
    float alphaPower = 0.f;
    float betaPower = 0.f;
    float thetaPower = 0.f;
    float targetBeatFreq = 0.f;
    float confidence = 0.f;
};

/// EEG 状态预测器抽象接口（可对接 CNN-LSTM 等模型）
class IEEGStatePredictor {
public:
    virtual ~IEEGStatePredictor() = default;

    virtual std::optional<EEGStatePrediction> predict(
        const std::vector<std::vector<float>>& eegChannels,
        float sampleRateHz) = 0;

    virtual bool isRealtimeCapable() const { return false; }
};

}  // namespace binaural
