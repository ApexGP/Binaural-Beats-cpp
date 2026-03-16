#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include "period.hpp"
#include "pinkNoise.hpp"
#include "sinTable.hpp"

namespace binaural {

struct SynthesizerConfig {
    int sampleRate = 44100;
    int bufferFrames = 2048;
};

class Synthesizer
{
public:
    explicit Synthesizer(const SynthesizerConfig& config = {});

    // ── Thread-safe setters（GUI 线程调用）──────────────────────────────────
    void setProgram(const Program& program);
    void setVolumeMultiplier(float v);
    /// balance: -1=左, 0=中, 1=右
    void setBalance(float b);
    /// 异步 seek：下一个音频回调时生效
    void setPeriodElapsedSec(float sec);

    // ── Audio-thread-only（仅在音频回调中调用）──────────────────────────────
    /// 根据 period 内已用时间线性插值各 voice 频率（每 voice 独立）
    void skewVoices(float periodElapsedSec);
    /// 覆写频率（AI 模式，ParameterController 在音频回调中调用）
    void setFreqs(const std::vector<float>& freqs);
    /// 填充立体声交错 16-bit 样本 [L0,R0,L1,R1,...]
    void fillSamples(std::vector<int16_t>& outSamples);
    void advanceTime(float sec);

    // ── Thread-safe getters（原子读取，供 GUI 显示）──────────────────────────
    int currentPeriodIndex() const
    {
        return currentPeriodIndex_.load(std::memory_order_relaxed);
    }
    float periodElapsedSec() const
    {
        return periodElapsedSec_.load(std::memory_order_relaxed);
    }
    const SynthesizerConfig& config() const
    {
        return config_;
    }

    /// 当前 Period 指针（audio-thread-only）
    const Period* currentPeriod() const;

private:
    float voicetoPitch(int voiceIndex) const;
    void ensureStateSize();
    /// 检查并应用 GUI 线程写入的 pending 更新，在 fillSamples 开头调用
    void applyPendingUpdates();

    SynthesizerConfig config_;

    // ── GUI→Audio 双缓冲：GUI 写 pending，音频回调检脏后 copy ──────────────
    mutable std::mutex programMutex_;
    Program pendingProgram_;                 // GUI 线程写
    std::atomic<bool> programDirty_{false};  // GUI 写 true，audio 消费后 false

    std::atomic<float> pendingBalance_{0.f};   // GUI 写
    std::atomic<float> pendingVolMult_{1.0f};  // GUI 写
    /// -1 表示无待处理 seek；>= 0 表示 GUI 请求 seek 到该秒数
    std::atomic<float> pendingSeekSec_{-1.f};

    // ── Audio-only 状态（仅音频回调线程访问，无需同步）─────────────────────
    Program program_;  // pendingProgram_ 的本地副本
    std::vector<float> freqs_;
    std::vector<float> vols_;
    std::vector<float> pitchs_;
    std::vector<bool> isochronic_;
    std::vector<float> phasesL_;
    std::vector<float> phasesR_;
    std::vector<float> phasesIso_;
    PinkNoise pinkNoise_;
    unsigned int whiteNoiseSeed_ = 1u;
    float balance_ = 0.f;            // 音频本地缓存
    float volumeMultiplier_ = 1.0f;  // 音频本地缓存

    // ── 显示用原子（音频写，GUI 读）────────────────────────────────────────
    std::atomic<int> currentPeriodIndex_{0};
    std::atomic<float> periodElapsedSec_{0.f};

    // ── 性能优化成员 ──────────────────────────────────────────────────────
    SinTable sinTable_;          // 快速正弦查找表（4096 项线性插值）
    std::vector<float> mixBuf_;  // 预分配混音缓冲，避免音频回调 malloc
    int lastEnsuredPidx_{-1};    // ensureStateSize 缓存，跳过逐帧重复检查
};

}  // namespace binaural
