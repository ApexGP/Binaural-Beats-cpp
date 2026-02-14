#pragma once

#include "period.hpp"
#include <cstdint>
#include <vector>

namespace binaural {

struct SynthesizerConfig {
    int sampleRate = 44100;
    int bufferFrames = 2048;
    int iscale = 1440;
};

class Synthesizer {
public:
    explicit Synthesizer(const SynthesizerConfig& config = {});

    void setProgram(const Program& program);
    void setFreqs(const std::vector<float>& freqs);
    void setVolumeMultiplier(float v);
    /// balance: -1=左, 0=中, 1=右
    void setBalance(float b);
    void skewVoices(float periodElapsedSec);

    /// 填充立体声交错 16-bit 样本 [L0,R0,L1,R1,...]
    void fillSamples(std::vector<int16_t>& outSamples);

    const SynthesizerConfig& config() const { return config_; }
    int currentPeriodIndex() const { return currentPeriodIndex_; }
    float periodElapsedSec() const { return periodElapsedSec_; }
    void advanceTime(float sec);

private:
    float voicetoPitch(int voiceIndex) const;
    void ensureStateSize();

    SynthesizerConfig config_;
    Program program_;

    std::vector<float> freqs_;
    std::vector<float> vols_;
    std::vector<float> pitchs_;
    std::vector<bool> isochronic_;
    std::vector<float> phasesL_;
    std::vector<float> phasesR_;
    std::vector<float> phasesIso_;

    int currentPeriodIndex_ = 0;
    float periodElapsedSec_ = 0.f;
    float volumeMultiplier_ = 1.0f;
    float balance_ = 0.f;  // -1..1
};

}  // namespace binaural
