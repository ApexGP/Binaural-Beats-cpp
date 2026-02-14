#pragma once

namespace binaural {

/// Trammell 粉红噪声 (1/f)，用于背景掩蔽
class PinkNoise {
public:
    PinkNoise();
    void clear();
    float tick();

private:
    static constexpr int NUM_STAGES = 3;
    float state_[NUM_STAGES];
    unsigned int seed_;
    static float randFloat(unsigned int& seed);
};

}  // namespace binaural
