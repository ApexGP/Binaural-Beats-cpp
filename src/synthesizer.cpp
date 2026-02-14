#include "binaural/synthesizer.hpp"
#include <algorithm>
#include <cmath>

namespace binaural {

namespace {
constexpr float A_FREQ = 432.0f;
constexpr float FADE_INOUT_PERIOD = 5.0f;
constexpr float FADE_MIN = 0.6f;

// voicetoPitch: Voice 索引 -> 基频 (Hz)
// 0:A4, 1:C4, 2:E4, 3:G4, 4:C5, 5:E6, 6+:A7
float getPitchFreq(int noteK, int octave) {
    return std::pow(2.0f, noteK / 12.0f + octave - 4) * A_FREQ;
}
}  // namespace

Synthesizer::Synthesizer(const SynthesizerConfig& config)
    : config_(config), sinTable_(config.iscale) {}

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
    const int iscale = config_.iscale;
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

    for (int j = 0; j < numVoices; ++j) {
        const float baseFreq = pitchs_[j];
        const float beatFreq = freqs_[j];
        const float vol = vols_[j] * fade * volumeMultiplier_;

        const int inc1 = static_cast<int>(
            std::round(iscale * (baseFreq + beatFreq) / sampleRate));
        const int inc2 =
            static_cast<int>(std::round(iscale * baseFreq / sampleRate));
        const int incIso =
            static_cast<int>(std::round(iscale * beatFreq / sampleRate));

        int angle1 = anglesL_[j];
        int angle2 = anglesR_[j];
        int angleIso = anglesIso_[j];

        if (isochronic_[j]) {
            for (int i = 0; i < numFrames * 2; i += 2) {
                if (angleIso < iscale / 2) {
                    ws[i] += sinTable_.sinFastInt(angle1) * vol;
                    ws[i + 1] += sinTable_.sinFastInt(angle2) * vol;
                }
                angle1 += inc1;
                angle2 += inc2;
                angleIso += incIso;
                if (angleIso >= iscale) angleIso -= iscale;
                if (angleIso < 0) angleIso += iscale;
            }
        } else {
            for (int i = 0; i < numFrames * 2; i += 2) {
                ws[i] += sinTable_.sinFastInt(angle1) * vol;
                ws[i + 1] += sinTable_.sinFastInt(angle2) * vol;
                angle1 += inc1;
                angle2 += inc2;
            }
        }

        anglesL_[j] = angle1 % iscale;
        if (anglesL_[j] < 0) anglesL_[j] += iscale;
        anglesR_[j] = angle2 % iscale;
        if (anglesR_[j] < 0) anglesR_[j] += iscale;
        anglesIso_[j] = angleIso;
    }

    const float multL = 1.0f - std::max(0.0f, balance_);
    const float multR = 1.0f - std::max(0.0f, -balance_);
    for (int i = 0; i < numFrames * 2; i += 2) {
        float valL = ws[i] * 32767.0f / numVoices * multL;
        float valR = ws[i + 1] * 32767.0f / numVoices * multR;
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
    if (vols_.size() != n) {
        vols_.resize(n);
        for (size_t j = 0; j < n; ++j) {
            vols_[j] = period.voices[j].volume;
        }
    }
    if (pitchs_.size() != n) {
        pitchs_.resize(n);
        for (size_t j = 0; j < n; ++j) {
            pitchs_[j] = period.voices[j].pitch < 0
                            ? voicetoPitch(static_cast<int>(j))
                            : period.voices[j].pitch;
        }
    }
    if (isochronic_.size() != n) {
        isochronic_.resize(n);
        for (size_t j = 0; j < n; ++j) {
            isochronic_[j] = period.voices[j].isochronic;
        }
    }
    if (anglesL_.size() != n) {
        anglesL_.resize(n, 0);
        anglesR_.resize(n, 0);
        anglesIso_.resize(n, 0);
    }
}

}  // namespace binaural
