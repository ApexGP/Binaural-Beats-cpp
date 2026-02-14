#include "binaural/stubPredictor.hpp"

namespace binaural {

StubPredictor::StubPredictor(bool simulate, float fixedTargetHz)
    : simulate_(simulate), fixedTargetHz_(fixedTargetHz) {}

std::optional<EEGStatePrediction> StubPredictor::predict(
    const std::vector<std::vector<float>>& /*eegChannels*/,
    float /*sampleRateHz*/) {
    if (!simulate_) {
        return std::nullopt;
    }
    EEGStatePrediction p;
    p.state = EEGStatePrediction::State::Relaxed;
    p.alphaPower = 0.5f;
    p.betaPower = 0.3f;
    p.thetaPower = 0.2f;
    p.targetBeatFreq = fixedTargetHz_;
    p.confidence = 0.8f;
    return p;
}

}  // namespace binaural
