#include "binaural/synthesizer.hpp"
#include <algorithm>
#include <cmath>

namespace binaural {

namespace {
constexpr float A_FREQ = 432.0f;
float whiteNoise(unsigned int& seed) {
    seed = seed * 1103515245u + 12345u;
    return ((seed >> 16) / 32768.0f) - 1.0f;
}
constexpr float TWO_PI = 6.283185307f;
constexpr float FADE_INOUT_PERIOD = 5.0f;
constexpr float FADE_MIN = 0.6f;

// voicetoPitch: Voice 索引 -> 基频 (Hz)
// 0:A4, 1:C4, 2:E4, 3:G4, 4:C5, 5:E6, 6+:A7
float getPitchFreq(int noteK, int octave) {
    return std::pow(2.0f, noteK / 12.0f + octave - 4) * A_FREQ;
}
}  // namespace

Synthesizer::Synthesizer(const SynthesizerConfig& config) : config_(config) {}

void Synthesizer::setProgram(const Program& program) {
    program_ = program;
    currentPeriodIndex_ = 0;
    periodElapsedSec_ = 0.f;
    ensureStateSize();
}

void Synthesizer::setFreqs(const std::vector<float>& freqs) {
    freqs_ = freqs;
}

void Synthesizer::setVolumeMultiplier(float v) {
    volumeMultiplier_ = std::clamp(v, 0.0f, 2.0f);
}

void Synthesizer::setBalance(float b) {
    balance_ = std::clamp(b, -1.0f, 1.0f);
}

void Synthesizer::skewVoices(float periodElapsedSec) {
    if (program_.seq.empty()) return;
    const Period& period = program_.seq[currentPeriodIndex_];
    if (period.voices.empty()) return;

    const int length = period.lengthSec;
    if (length <= 0) return;

    const float pos = periodElapsedSec;
    const auto& v0 = period.voices[0];
    const float ratio = (v0.freqEnd - v0.freqStart) / length;
    const float res = ratio * pos + v0.freqStart;

    freqs_.resize(period.voices.size());
    for (size_t j = 0; j < period.voices.size(); ++j) {
        freqs_[j] = res;
    }
}

void Synthesizer::fillSamples(std::vector<int16_t>& outSamples) {
    outSamples.resize(config_.bufferFrames * 2);

    if (program_.seq.empty()) {
        std::fill(outSamples.begin(), outSamples.end(), 0);
        return;
    }

    const Period& period = program_.seq[currentPeriodIndex_];
    if (period.voices.empty()) {
        std::fill(outSamples.begin(), outSamples.end(), 0);
        return;
    }

    ensureStateSize();

    const int numFrames = config_.bufferFrames;
    const float sampleRate = static_cast<float>(config_.sampleRate);
    const int numVoices = static_cast<int>(period.voices.size());

    float fade = 1.0f;
    if (period.lengthSec >= FADE_INOUT_PERIOD) {
        const float fadePeriod = std::min(FADE_INOUT_PERIOD / 2.0f,
                                          period.lengthSec / 2.0f);
        if (periodElapsedSec_ < fadePeriod) {
            fade = FADE_MIN + (periodElapsedSec_ / fadePeriod) * (1.0f - FADE_MIN);
        } else if (period.lengthSec - periodElapsedSec_ < fadePeriod) {
            fade = FADE_MIN + ((period.lengthSec - periodElapsedSec_) / fadePeriod) *
                                 (1.0f - FADE_MIN);
        }
    }

    std::vector<float> ws(numFrames * 2, 0.f);

    const float phaseStepScale = 1.0f / sampleRate;
    for (int j = 0; j < numVoices; ++j) {
        const float baseFreq = pitchs_[j];
        const float beatFreq = freqs_[j];
        const float vol = vols_[j] * fade * volumeMultiplier_;

        const float incL = (baseFreq + beatFreq) * phaseStepScale;
        const float incR = baseFreq * phaseStepScale;
        const float incIso = beatFreq * phaseStepScale;

        float phaseL = phasesL_[j];
        float phaseR = phasesR_[j];
        float phaseIso = phasesIso_[j];

        if (isochronic_[j]) {
            for (int i = 0; i < numFrames * 2; i += 2) {
                float gain = 0.f;
                if (phaseIso < 0.5f) {
                    gain = std::cos(phaseIso * 3.14159265f);
                }
                ws[i] += std::sin(TWO_PI * phaseL) * vol * gain;
                ws[i + 1] += std::sin(TWO_PI * phaseR) * vol * gain;
                phaseL += incL;
                phaseR += incR;
                if (phaseL >= 1.0f) phaseL -= 1.0f;
                if (phaseR >= 1.0f) phaseR -= 1.0f;
                phaseIso += incIso;
                if (phaseIso >= 1.0f) phaseIso -= 1.0f;
                if (phaseIso < 0.0f) phaseIso += 1.0f;
            }
        } else {
            for (int i = 0; i < numFrames * 2; i += 2) {
                ws[i] += std::sin(TWO_PI * phaseL) * vol;
                ws[i + 1] += std::sin(TWO_PI * phaseR) * vol;
                phaseL += incL;
                phaseR += incR;
                if (phaseL >= 1.0f) phaseL -= 1.0f;
                if (phaseR >= 1.0f) phaseR -= 1.0f;
            }
        }
        phasesL_[j] = phaseL;
        phasesR_[j] = phaseR;
        phasesIso_[j] = phaseIso;
    }

    const float multL = 1.0f - std::max(0.0f, balance_);
    const float multR = 1.0f - std::max(0.0f, -balance_);
    const float bgVol = period.backgroundVol * fade * volumeMultiplier_ * 0.5f;
    const bool usePink = (period.background == Period::Background::PinkNoise);
    const bool useWhite = (period.background == Period::Background::WhiteNoise);

    for (int i = 0; i < numFrames * 2; i += 2) {
        float valL = ws[i] * 32767.0f / numVoices * multL;
        float valR = ws[i + 1] * 32767.0f / numVoices * multR;
        if (bgVol > 0.f) {
            if (usePink) {
                const float p = pinkNoise_.tick();
                valL += p * bgVol * 32767.0f * multL;
                valR += p * bgVol * 32767.0f * multR;
            } else if (useWhite) {
                const float w = whiteNoise(whiteNoiseSeed_);
                valL += w * bgVol * 32767.0f * multL;
                valR += w * bgVol * 32767.0f * multR;
            }
        }
        outSamples[i] = static_cast<int16_t>(
            std::clamp(valL, -32768.0f, 32767.0f));
        outSamples[i + 1] = static_cast<int16_t>(
            std::clamp(valR, -32768.0f, 32767.0f));
    }
}

void Synthesizer::advanceTime(float sec) {
    periodElapsedSec_ += sec;
    if (program_.seq.empty()) return;

    Period& period = program_.seq[currentPeriodIndex_];
    if (periodElapsedSec_ >= period.lengthSec) {
        periodElapsedSec_ -= period.lengthSec;
        currentPeriodIndex_ = (currentPeriodIndex_ + 1) % program_.seq.size();
        if (currentPeriodIndex_ == 0 && program_.seq.size() > 1) {
            periodElapsedSec_ = 0.f;
        }
    }
}

void Synthesizer::setPeriodElapsedSec(float sec) {
    if (program_.seq.empty()) return;
    const Period& period = program_.seq[currentPeriodIndex_];
    periodElapsedSec_ =
        std::clamp(sec, 0.f, static_cast<float>(period.lengthSec));
    skewVoices(periodElapsedSec_);
}

float Synthesizer::voicetoPitch(int voiceIndex) const {
    switch (voiceIndex) {
        case 0:
            return getPitchFreq(0, 4);   // A4
        case 1:
            return getPitchFreq(3, 4);   // C4
        case 2:
            return getPitchFreq(7, 4);   // E4
        case 3:
            return getPitchFreq(10, 4);  // G4
        case 4:
            return getPitchFreq(3, 5);   // C5
        case 5:
            return getPitchFreq(7, 6);   // E6
        default:
            return getPitchFreq(0, 7);  // A7
    }
}

const Period* Synthesizer::currentPeriod() const {
    if (program_.seq.empty()) return nullptr;
    if (currentPeriodIndex_ < 0 ||
        static_cast<size_t>(currentPeriodIndex_) >= program_.seq.size())
        return nullptr;
    return &program_.seq[currentPeriodIndex_];
}

void Synthesizer::ensureStateSize() {
    if (program_.seq.empty()) return;
    const Period& period = program_.seq[currentPeriodIndex_];
    const size_t n = period.voices.size();

    if (freqs_.size() != n) {
        freqs_.resize(n);
        for (size_t j = 0; j < n; ++j) {
            freqs_[j] = period.voices[j].freqStart;
        }
    }
    if (vols_.size() != n) vols_.resize(n);
    for (size_t j = 0; j < n; ++j) {
        vols_[j] = period.voices[j].volume;
    }
    if (pitchs_.size() != n) pitchs_.resize(n);
    for (size_t j = 0; j < n; ++j) {
        pitchs_[j] = period.voices[j].pitch < 0
                        ? voicetoPitch(static_cast<int>(j))
                        : period.voices[j].pitch;
    }
    if (isochronic_.size() != n) {
        isochronic_.resize(n);
        for (size_t j = 0; j < n; ++j) {
            isochronic_[j] = period.voices[j].isochronic;
        }
    }
    if (phasesL_.size() != n) {
        phasesL_.resize(n, 0.f);
        phasesR_.resize(n, 0.f);
        phasesIso_.resize(n, 0.f);
    }
}

}  // namespace binaural
