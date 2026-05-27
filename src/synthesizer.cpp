#include "binaural/synthesizer.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

namespace binaural {

namespace {
constexpr float A_FREQ = 432.0f;
float whiteNoise(unsigned int &seed)
{
    seed = seed * 1103515245u + 12345u;
    return ((seed >> 16) / 32768.0f) - 1.0f;
}
constexpr float TWO_PI = 6.283185307f;
constexpr float FADE_INOUT_PERIOD = 5.0f;
constexpr float FADE_MIN = 0.6f;

// voicetoPitch: Voice 索引 -> 基频 (Hz)
// 0:A4, 1:C4, 2:E4, 3:G4, 4:C5, 5:E6, 6+:A7
float getPitchFreq(int noteK, int octave)
{
    return std::pow(2.0f, noteK / 12.0f + octave - 4) * A_FREQ;
}
}  // namespace

Synthesizer::Synthesizer(const SynthesizerConfig &config)
    : config_(config), mixBuf_(config.bufferFrames * 2, 0.f)
{
}

// ── Thread-safe setters ──────────────────────────────────────────────────────

void Synthesizer::setProgram(const Program &program)
{
    {
        std::lock_guard<std::mutex> lock(programMutex_);
        pendingProgram_ = program;
    }
    programDirty_.store(true, std::memory_order_release);
}

void Synthesizer::setVolumeMultiplier(float v)
{
    pendingVolMult_.store(std::clamp(v, 0.0f, 2.0f), std::memory_order_relaxed);
}

void Synthesizer::setBalance(float b)
{
    pendingBalance_.store(std::clamp(b, -1.0f, 1.0f), std::memory_order_relaxed);
}

void Synthesizer::setPeriodElapsedSec(float sec)
{
    // 先用 pendingProgram_ 的当前 period 做 clamp（近似值，够用）
    float clampedSec = sec;
    {
        std::lock_guard<std::mutex> lock(programMutex_);
        int idx = currentPeriodIndex_.load(std::memory_order_relaxed);
        if (!pendingProgram_.seq.empty()) {
            if (idx < 0 || idx >= static_cast<int>(pendingProgram_.seq.size())) idx = 0;
            clampedSec =
                std::clamp(sec, 0.f, static_cast<float>(pendingProgram_.seq[idx].lengthSec));
        }
    }
    pendingSeekSec_.store(clampedSec, std::memory_order_release);
}

// ── Audio-thread-only ────────────────────────────────────────────────────────

void Synthesizer::setFreqs(const std::vector<float> &freqs)
{
    freqs_ = freqs;
}

void Synthesizer::skewVoices(float periodElapsedSec)
{
    if (program_.seq.empty()) return;
    int pidx = currentPeriodIndex_.load(std::memory_order_relaxed);
    if (pidx < 0 || pidx >= static_cast<int>(program_.seq.size())) return;
    const Period &period = program_.seq[pidx];
    if (period.voices.empty()) return;

    const float length = static_cast<float>(period.lengthSec);
    if (length <= 0.f) return;

    freqs_.resize(period.voices.size());
    for (size_t j = 0; j < period.voices.size(); ++j) {
        // 每个 voice 使用各自的 freqStart/freqEnd 独立插值（M1 修复）
        const auto &v = period.voices[j];
        const float ratio = (v.freqEnd - v.freqStart) / length;
        freqs_[j] = ratio * periodElapsedSec + v.freqStart;
    }
}

void Synthesizer::applyPendingUpdates()
{
    // 应用 program 更新
    if (programDirty_.load(std::memory_order_acquire)) {
        Program newProgram;
        {
            std::lock_guard<std::mutex> lock(programMutex_);
            newProgram = pendingProgram_;
        }
        programDirty_.store(false, std::memory_order_release);

        // 判断结构是否改变（period数量或当前period的voice数量）
        // 纯参数变化（beatFreq/baseFreq/volume/isochronic）结构不变，保留相位避免咔哒声
        const int pidx = currentPeriodIndex_.load(std::memory_order_relaxed);
        const bool sameStructure =
            !program_.seq.empty() && newProgram.seq.size() == program_.seq.size() && pidx >= 0 &&
            pidx < static_cast<int>(newProgram.seq.size()) &&
            !newProgram.seq[pidx].voices.empty() &&
            newProgram.seq[pidx].voices.size() == program_.seq[pidx].voices.size();

        program_ = std::move(newProgram);

        // 频率/音量/音调需要重新读取，清空后 ensureStateSize 会从新 program 填充
        freqs_.clear();
        vols_.clear();
        pitchs_.clear();
        isochronic_.clear();
        lastEnsuredPidx_ = -1;  // O5: 强制下次 ensureStateSize 重新初始化

        if (!sameStructure) {
            // 结构变化：重置全部音频状态（包括相位和进度）
            currentPeriodIndex_.store(0, std::memory_order_relaxed);
            periodElapsedSec_.store(0.f, std::memory_order_relaxed);
            phasesL_.clear();
            phasesR_.clear();
            phasesIso_.clear();
        }
        // sameStructure：保留 phasesL/R/Iso、currentPeriodIndex_、periodElapsedSec_
    }

    // 应用 seek 请求
    float seekSec = pendingSeekSec_.exchange(-1.f, std::memory_order_acq_rel);
    if (seekSec >= 0.f) {
        int pidx = currentPeriodIndex_.load(std::memory_order_relaxed);
        if (!program_.seq.empty()) {
            if (pidx < 0 || pidx >= static_cast<int>(program_.seq.size())) pidx = 0;
            float maxSec = static_cast<float>(program_.seq[pidx].lengthSec);
            seekSec = std::clamp(seekSec, 0.f, maxSec);
        }
        periodElapsedSec_.store(seekSec, std::memory_order_relaxed);
        skewVoices(seekSec);
    }

    // 应用简单参数
    balance_ = pendingBalance_.load(std::memory_order_relaxed);
    volumeMultiplier_ = pendingVolMult_.load(std::memory_order_relaxed);
}

void Synthesizer::fillSamples(std::vector<int16_t> &outSamples)
{
    // 在每个 buffer 开头同步 GUI 写入的参数（双缓冲核心）
    applyPendingUpdates();

    outSamples.resize(config_.bufferFrames * 2);

    if (program_.seq.empty()) {
        std::fill(outSamples.begin(), outSamples.end(), 0);
        return;
    }

    int pidx = currentPeriodIndex_.load(std::memory_order_relaxed);
    if (pidx < 0 || pidx >= static_cast<int>(program_.seq.size())) pidx = 0;
    const Period &period = program_.seq[pidx];

    if (period.voices.empty()) {
        std::fill(outSamples.begin(), outSamples.end(), 0);
        return;
    }

    ensureStateSize();

    const int numFrames = config_.bufferFrames;
    const float sampleRate = static_cast<float>(config_.sampleRate);
    const int numVoices = static_cast<int>(period.voices.size());
    const float pelapsed = periodElapsedSec_.load(std::memory_order_relaxed);

    float fade = 1.0f;
    if (period.lengthSec >= FADE_INOUT_PERIOD) {
        const float fadePeriod = std::min(FADE_INOUT_PERIOD / 2.0f, period.lengthSec / 2.0f);
        if (pelapsed < fadePeriod) {
            fade = FADE_MIN + (pelapsed / fadePeriod) * (1.0f - FADE_MIN);
        } else if (period.lengthSec - pelapsed < fadePeriod) {
            fade = FADE_MIN + ((period.lengthSec - pelapsed) / fadePeriod) * (1.0f - FADE_MIN);
        }
    }

    // P1: 首个 voice 直接赋值 mixBuf_，省去 std::fill 清零（单 voice 场景收益最大）
    // 后续 voice 用累加（+=），保持多 voice 叠加语义
    const float phaseStepScale = 1.0f / sampleRate;
    for (int j = 0; j < numVoices; ++j) {
        const float baseFreq = pitchs_[j];
        const float beatFreq = freqs_[j];
        const float vol = vols_[j] * fade * volumeMultiplier_;

        float phaseL = phasesL_[j];
        float phaseR = phasesR_[j];
        float phaseIso = phasesIso_[j];

        if (isochronic_[j]) {
            const float incCarrier = baseFreq * phaseStepScale;
            const float incIso = beatFreq * phaseStepScale;
            if (j == 0) {
                for (int i = 0; i < numFrames * 2; i += 2) {
                    float gain = 0.f;
                    if (phaseIso < 0.5f) {
                        gain = sinTable_.sinFastFloat(phaseIso * 0.5f + 0.25f);
                    }
                    const float s = sinTable_.sinFastFloat(phaseL) * vol * gain;
                    mixBuf_[i] = s;
                    mixBuf_[i + 1] = s;
                    phaseL += incCarrier;
                    if (phaseL >= 1.0f) phaseL -= 1.0f;
                    phaseIso += incIso;
                    if (phaseIso >= 1.0f) phaseIso -= 1.0f;
                }
            } else {
                for (int i = 0; i < numFrames * 2; i += 2) {
                    float gain = 0.f;
                    if (phaseIso < 0.5f) {
                        gain = sinTable_.sinFastFloat(phaseIso * 0.5f + 0.25f);
                    }
                    const float s = sinTable_.sinFastFloat(phaseL) * vol * gain;
                    mixBuf_[i] += s;
                    mixBuf_[i + 1] += s;
                    phaseL += incCarrier;
                    if (phaseL >= 1.0f) phaseL -= 1.0f;
                    phaseIso += incIso;
                    if (phaseIso >= 1.0f) phaseIso -= 1.0f;
                }
            }
            phasesL_[j] = phaseL;
            phasesR_[j] = phaseL;
            phasesIso_[j] = phaseIso;
        } else {
            const float incL = (baseFreq + beatFreq) * phaseStepScale;
            const float incR = baseFreq * phaseStepScale;
            if (j == 0) {
                for (int i = 0; i < numFrames * 2; i += 2) {
                    mixBuf_[i] = sinTable_.sinFastFloat(phaseL) * vol;
                    mixBuf_[i + 1] = sinTable_.sinFastFloat(phaseR) * vol;
                    phaseL += incL;
                    phaseR += incR;
                    if (phaseL >= 1.0f) phaseL -= 1.0f;
                    if (phaseR >= 1.0f) phaseR -= 1.0f;
                }
            } else {
                for (int i = 0; i < numFrames * 2; i += 2) {
                    mixBuf_[i] += sinTable_.sinFastFloat(phaseL) * vol;
                    mixBuf_[i + 1] += sinTable_.sinFastFloat(phaseR) * vol;
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
    }

    // P2/P3: 分离噪声/平衡路径 — 默认无噪声+balance居中走最快路径
    const float volScale = 32767.0f / numVoices;
    const float bgVol = period.backgroundVol * fade * volumeMultiplier_ * 0.5f;
    const bool usePink = (period.background == Period::Background::PinkNoise);
    const bool useWhite = (period.background == Period::Background::WhiteNoise);

    if (bgVol > 0.f) {
        // 噪声路径
        const float multL = 1.0f - std::max(0.0f, balance_);
        const float multR = 1.0f - std::max(0.0f, -balance_);
        for (int i = 0; i < numFrames * 2; i += 2) {
            float valL = mixBuf_[i] * volScale * multL;
            float valR = mixBuf_[i + 1] * volScale * multR;
            if (usePink) {
                const float p = pinkNoise_.tick();
                valL += p * bgVol * 32767.0f * multL;
                valR += p * bgVol * 32767.0f * multR;
            } else {
                const float w = whiteNoise(whiteNoiseSeed_);
                valL += w * bgVol * 32767.0f * multL;
                valR += w * bgVol * 32767.0f * multR;
            }
            outSamples[i] = static_cast<int16_t>(std::clamp(valL, -32768.0f, 32767.0f));
            outSamples[i + 1] = static_cast<int16_t>(std::clamp(valR, -32768.0f, 32767.0f));
        }
    } else if (balance_ != 0.f) {
        // 无噪声 + 非居中平衡
        const float multL = 1.0f - std::max(0.0f, balance_);
        const float multR = 1.0f - std::max(0.0f, -balance_);
        for (int i = 0; i < numFrames * 2; i += 2) {
            outSamples[i] = static_cast<int16_t>(
                std::clamp(mixBuf_[i] * volScale * multL, -32768.0f, 32767.0f));
            outSamples[i + 1] = static_cast<int16_t>(
                std::clamp(mixBuf_[i + 1] * volScale * multR, -32768.0f, 32767.0f));
        }
    } else {
        // 最快路径：无噪声 + balance 居中
        for (int i = 0; i < numFrames * 2; i += 2) {
            outSamples[i] =
                static_cast<int16_t>(std::clamp(mixBuf_[i] * volScale, -32768.0f, 32767.0f));
            outSamples[i + 1] =
                static_cast<int16_t>(std::clamp(mixBuf_[i + 1] * volScale, -32768.0f, 32767.0f));
        }
    }
}

void Synthesizer::advanceTime(float sec)
{
    if (program_.seq.empty()) return;

    float pelapsed = periodElapsedSec_.load(std::memory_order_relaxed);
    int pidx = currentPeriodIndex_.load(std::memory_order_relaxed);
    pelapsed += sec;

    // while 循环处理单帧内跨越多个 period 的情况（L4 修复）
    while (!program_.seq.empty()) {
        if (pidx < 0 || pidx >= static_cast<int>(program_.seq.size())) {
            pidx = 0;
            pelapsed = 0.f;
            break;
        }
        const Period &period = program_.seq[pidx];
        if (period.lengthSec <= 0) {
            pelapsed = 0.f;
            break;
        }  // 防止死循环
        if (pelapsed < period.lengthSec) break;
        pelapsed -= period.lengthSec;
        pidx = (pidx + 1) % static_cast<int>(program_.seq.size());
        if (pidx == 0) pelapsed = 0.f;  // 回绕时强制清零，防止累积误差
    }

    periodElapsedSec_.store(pelapsed, std::memory_order_relaxed);
    currentPeriodIndex_.store(pidx, std::memory_order_relaxed);
}

float Synthesizer::voicetoPitch(int voiceIndex) const
{
    switch (voiceIndex) {
        case 0:
            return getPitchFreq(0, 4);  // A4
        case 1:
            return getPitchFreq(3, 4);  // C4
        case 2:
            return getPitchFreq(7, 4);  // E4
        case 3:
            return getPitchFreq(10, 4);  // G4
        case 4:
            return getPitchFreq(3, 5);  // C5
        case 5:
            return getPitchFreq(7, 6);  // E6
        default:
            return getPitchFreq(0, 7);  // A7
    }
}

const Period *Synthesizer::currentPeriod() const
{
    // audio-thread-only：直接访问音频本地 program_
    if (program_.seq.empty()) return nullptr;
    int pidx = currentPeriodIndex_.load(std::memory_order_relaxed);
    if (pidx < 0 || static_cast<size_t>(pidx) >= program_.seq.size()) return nullptr;
    return &program_.seq[pidx];
}

void Synthesizer::ensureStateSize()
{
    if (program_.seq.empty()) return;
    int pidx = currentPeriodIndex_.load(std::memory_order_relaxed);
    if (pidx < 0 || pidx >= static_cast<int>(program_.seq.size())) return;
    const Period &period = program_.seq[pidx];
    const size_t n = period.voices.size();

    // 若 pidx 与上次相同且所有 vector 尺寸正确，直接跳过
    if (pidx == lastEnsuredPidx_ && freqs_.size() == n && vols_.size() == n &&
        pitchs_.size() == n && isochronic_.size() == n && phasesL_.size() == n) {
        return;
    }

    if (freqs_.size() != n) {
        freqs_.resize(n);
        for (size_t j = 0; j < n; ++j) freqs_[j] = period.voices[j].freqStart;
    }
    // vols_ 和 pitchs_ 仅在 size 变化时更新，避免覆盖运行时修改
    if (vols_.size() != n) {
        vols_.resize(n);
        for (size_t j = 0; j < n; ++j) vols_[j] = period.voices[j].volume;
    }
    if (pitchs_.size() != n) {
        pitchs_.resize(n);
        for (size_t j = 0; j < n; ++j)
            pitchs_[j] = period.voices[j].pitch < 0 ? voicetoPitch(static_cast<int>(j))
                                                    : period.voices[j].pitch;
    }
    if (isochronic_.size() != n) {
        isochronic_.resize(n);
        for (size_t j = 0; j < n; ++j) isochronic_[j] = period.voices[j].isochronic;
    }
    if (phasesL_.size() != n) {
        phasesL_.resize(n, 0.f);
        phasesR_.resize(n, 0.f);
        phasesIso_.resize(n, 0.f);
    }

    // 确保 mixBuf_ 尺寸正确（通常已在构造时分配，此处为保险）
    if (mixBuf_.size() != static_cast<size_t>(config_.bufferFrames * 2))
        mixBuf_.resize(config_.bufferFrames * 2, 0.f);

    lastEnsuredPidx_ = pidx;  // 缓存当前 pidx，下次同 period 直接跳过
}

}  // namespace binaural
