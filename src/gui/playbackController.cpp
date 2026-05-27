#include "gui/playbackController.hpp"

#include <vector>

#include "binaural/gnauralParser.hpp"
#include "binaural/period.hpp"

namespace gui {

void PlaybackController::start(AppContext &ctx)
{
    if (ctx.playing) return;
    bool ok = ctx.driver->start(
        ctx.config.sampleRate, ctx.config.bufferFrames, [&ctx](std::vector<int16_t> &buf) {
            float delta = static_cast<float>(ctx.config.bufferFrames) / ctx.config.sampleRate;
            ctx.paramController.update(ctx.synth.periodElapsedSec());
            ctx.synth.fillSamples(buf);
            for (size_t i = 0; i < buf.size(); i += 4) {
                float l = buf[i] * (1.f / 32768.f);
                float r = buf[i + 1] * (1.f / 32768.f);
                ctx.waveBuf.push(l, r);
            }
            ctx.synth.advanceTime(delta);
            if (!ctx.loadedFromGnaural) {
                float cur = ctx.manualElapsedSec.load(std::memory_order_relaxed);
                ctx.manualElapsedSec.store(cur + delta, std::memory_order_relaxed);
            }
        });
    if (ok) ctx.playing = true;
}

void PlaybackController::stop(AppContext &ctx)
{
    if (!ctx.playing) return;
    const bool wasAiDriven = ctx.paramController.isAiDriven();
    if (wasAiDriven) {
        ctx.beatFreq = ctx.paramController.currentBeatFreq();
        int idx = ctx.synth.currentPeriodIndex();
        if (!ctx.program.seq.empty() && idx < static_cast<int>(ctx.program.seq.size()) &&
            !ctx.program.seq[idx].voices.empty()) {
            ctx.program.seq[idx].voices[0].freqStart = ctx.beatFreq;
            ctx.program.seq[idx].voices[0].freqEnd = ctx.beatFreq;
            ctx.synth.setProgram(ctx.program);
        }
        ctx.manualElapsedSec.store(0.f, std::memory_order_relaxed);
    }
    ctx.paramController.clearAiState();
    ctx.driver->stop();
    ctx.playing = false;
}

void PlaybackController::returnToManual(AppContext &ctx)
{
    const bool wasAiDriven = ctx.paramController.isAiDriven();
    if (wasAiDriven) {
        ctx.beatFreq = ctx.paramController.currentBeatFreq();
        int idx = ctx.synth.currentPeriodIndex();
        if (!ctx.program.seq.empty() && idx < static_cast<int>(ctx.program.seq.size()) &&
            !ctx.program.seq[idx].voices.empty()) {
            ctx.program.seq[idx].voices[0].freqStart = ctx.beatFreq;
            ctx.program.seq[idx].voices[0].freqEnd = ctx.beatFreq;
            ctx.synth.setProgram(ctx.program);
        }
        ctx.paramController.clearAiState();
    }
    if (ctx.loadedFromGnaural && !wasAiDriven) {
        ctx.program = binaural::Program{};
        ctx.program.name = "Theta meditation";
        ctx.program.seq.push_back({
            .lengthSec = 3600,
            .voices = {{.freqStart = 4.f,
                        .freqEnd = 4.f,
                        .volume = 0.7f,
                        .pitch = 161.f,
                        .isochronic = false}},
            .background = binaural::Period::Background::None,
            .backgroundVol = 0.f,
        });
        ctx.synth.setProgram(ctx.program);
        ctx.loadedFromGnaural = false;
        ctx.beatFreq = 4.f;
        ctx.baseFreq = 161.f;
    }
    ctx.manualElapsedSec.store(0.f, std::memory_order_relaxed);
}

void PlaybackController::exitTimedPlayback(AppContext &ctx)
{
    ctx.timedPlaybackEnabled = false;
    ctx.manualElapsedSec.store(0.f, std::memory_order_relaxed);
}

bool PlaybackController::loadGnaural(AppContext &ctx)
{
    auto prog = binaural::parseGnaural(ctx.loadPathBuf);
    if (!prog) return false;
    ctx.program = std::move(*prog);
    ctx.synth.setProgram(ctx.program);
    ctx.loadedFromGnaural = true;
    if (!ctx.program.seq.empty() && !ctx.program.seq[0].voices.empty()) {
        ctx.beatFreq = ctx.program.seq[0].voices[0].freqStart;
        ctx.baseFreq =
            ctx.program.seq[0].voices[0].pitch > 0 ? ctx.program.seq[0].voices[0].pitch : 161.f;
    }
    return true;
}

}  // namespace gui
