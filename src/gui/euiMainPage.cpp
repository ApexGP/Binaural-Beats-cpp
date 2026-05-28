// euiMainPage.cpp — 主页面 compose 函数

#include "gui/guiPanels.hpp"
#include "gui/guiUtils.hpp"
#include "gui/playbackController.hpp"
#include "gui/euiComponents.h"

#include "binaural/eegPredictorInterface.hpp"
#include "binaural/period.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace gui {

namespace {

// ── 布局常量 ──────────────────────────────────────────────────────────
constexpr float PAD_BASE = 20.f;
constexpr float TITLE_H_BASE = 48.f;
constexpr float WAVE_H_BASE = 140.f;
constexpr float DESC_H_BASE = 44.f;
constexpr float ROW_H_BASE = 36.f;

bool g_bgDropdownOpen = false;

}  // namespace

void composeTitleBar(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx)
{
    const float s = ctx.uiScale;
    const float pad = PAD_BASE * s;
    const float titleH = TITLE_H_BASE * s;
    const float btnSz = 34.f * s;
    const float menuBtnW = 34.f * s;
    const float menuBtnReserve = 56.f * s;
    const float durBlockW = 210.f * s;
    const float contentW = screen.width - pad * 2.f;

    // 时长文本准备（与 ImGui 版本逻辑一致）
    char eBuf[16], tBuf[16];
    static float frozenManualElapsed = 0.f;
    static bool modalWasOpen = false;
    if (ctx.modalOpen) {
        if (!modalWasOpen)
            frozenManualElapsed = ctx.manualElapsedSec.load(std::memory_order_relaxed);
        modalWasOpen = true;
    } else {
        modalWasOpen = false;
    }
    const bool modalOpen = ctx.modalOpen;

    if (ctx.loadedFromGnaural && !ctx.program.seq.empty()) {
        int idx = ctx.synth.currentPeriodIndex();
        if (idx >= static_cast<int>(ctx.program.seq.size())) idx = 0;
        snprintf(eBuf, sizeof(eBuf), "%d", static_cast<int>(ctx.synth.periodElapsedSec() + 0.5f));
        snprintf(tBuf, sizeof(tBuf), "%d", ctx.program.seq[idx].lengthSec);
    } else {
        const float elapsedForDisplay =
            modalOpen ? frozenManualElapsed : ctx.manualElapsedSec.load(std::memory_order_relaxed);
        snprintf(eBuf, sizeof(eBuf), "%d", static_cast<int>(elapsedForDisplay + 0.5f));
        if (ctx.timedPlaybackEnabled) {
            snprintf(tBuf, sizeof(tBuf), "%d",
                     static_cast<int>(ctx.timedPlaybackDurationSec + 0.5f));
        } else {
            snprintf(tBuf, sizeof(tBuf), "\xE2\x88\x9E");  // ∞
        }
    }

    ui.stack("titleBar.stack")
        .x(pad)
        .y(pad)
        .size(contentW, titleH)
        .content([&] {
            // 标题（底层，不拦截按钮点击）
            ui.text("titleBar.title")
                .x(0.f)
                .y(0.f)
                .size(contentW, titleH)
                .text("Binaural Beats")
                .fontSize(21.f * s)
                .color({0.94f, 0.97f, 1.0f, 1.0f})
                .horizontalAlign(core::HorizontalAlign::Center)
                .verticalAlign(core::VerticalAlign::Center)
                .build();

            // ? 帮助按钮（左上，高层级）
            ui.stack("titleBar.helpWrap")
                .x(0.f)
                .y((titleH - btnSz) * 0.5f)
                .size(btnSz, btnSz)
                .zIndex(20)
                .content([&] {
                    components::button(ui, "titleBar.help")
                        .size(btnSz, btnSz)
                        .text("\xEF\x81\x99")  // fa-question-circle
                        .fontSize(24.f * s)
                        .radius(btnSz * 0.5f)
                        .secondaryTheme(components::theme::DarkThemeColors())
                        .onClick([&] { ctx.showHelpCenter = true; })
                        .build();
                })
                .build();

            // 时长区块（右侧）
            const float durX = contentW - menuBtnReserve - durBlockW;
            ui.row("titleBar.duration")
                .x(durX)
                .y(0.f)
                .size(durBlockW, titleH)
                .alignItems(core::Align::CENTER)
                .gap(2.f * s)
                .zIndex(5)
                .content([&] {
                    ui.text("titleBar.dur.label")
                        .size(74.f * s, titleH)
                        .text("duration")
                        .fontSize(18.f * s)
                        .color({0.4f, 0.7f, 1.0f, 1.0f})
                        .verticalAlign(core::VerticalAlign::Center)
                        .build();

                    ui.text("titleBar.dur.elapsed")
                        .size(30.f * s, titleH)
                        .text(eBuf)
                        .fontSize(15.f * s)
                        .color({0.3f, 0.85f, 0.42f, 1.0f})
                        .verticalAlign(core::VerticalAlign::Center)
                        .build();

                    ui.text("titleBar.dur.sec")
                        .size(16.f * s, titleH)
                        .text("s")
                        .fontSize(18.f * s)
                        .color({0.50f, 0.55f, 0.64f, 1.0f})
                        .verticalAlign(core::VerticalAlign::Center)
                        .build();

                    const char* slashFmt =
                        (ctx.loadedFromGnaural || ctx.timedPlaybackEnabled) ? " / %s s" : " / %s";
                    char slashBuf[32];
                    snprintf(slashBuf, sizeof(slashBuf), slashFmt, tBuf);
                    ui.text("titleBar.dur.total")
                        .size(70.f * s, titleH)
                        .text(slashBuf)
                        .fontSize(18.f * s)
                        .color({0.50f, 0.55f, 0.64f, 1.0f})
                        .verticalAlign(core::VerticalAlign::Center)
                        .build();
                })
                .build();

            // ⋮ 菜单按钮（右上，高层级）
            ui.stack("titleBar.menuBtnWrapper")
                .x(contentW - menuBtnW - 4.f * s)
                .y((titleH - btnSz) * 0.5f)
                .size(menuBtnW, btnSz)
                .zIndex(20)
                .content([&] {
                    components::button(ui, "titleBar.menuBtn")
                        .size(menuBtnW, btnSz)
                        .text("\xEF\x83\x89")  // fa-bars
                        .fontSize(26.f * s)
                        .radius(btnSz * 0.5f)
                        .secondaryTheme(components::theme::DarkThemeColors())
                        .onClick([&] { ctx.showMenu = !ctx.showMenu; })
                        .build();
                })
                .build();
        })
        .build();
}

void composeWaveform(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx)
{
    const float s = ctx.uiScale;
    const float pad = PAD_BASE * s;
    const float titleH = TITLE_H_BASE * s;
    const float waveH = WAVE_H_BASE * s;
    const float y = pad + titleH;
    const float contentW = screen.width - pad * 2.f;

    ui.stack("waveform.stack")
        .x(pad)
        .y(y)
        .size(contentW, waveH)
        .content([&] {
            std::vector<float> samplesL, samplesR;
            ctx.waveBuf.getSamples(samplesL, samplesR);
            if (!samplesL.empty()) {
                waveformDisplay(ui, "waveform.display", samplesL, samplesR, contentW, waveH, s);
            } else {
                ui.text("waveform.placeholder")
                    .x(contentW / 2.f - 50.f * s)
                    .y(waveH / 2.f - 10.f * s)
                    .text("Waveform")
                    .fontSize(17.f * s)
                    .color({0.4f, 0.4f, 0.45f, 1.0f})
                    .horizontalAlign(core::HorizontalAlign::Center)
                    .build();
            }
        })
        .build();
}

void composeBeatDescription(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx)
{
    const float s = ctx.uiScale;
    const float pad = PAD_BASE * s;
    const float titleH = TITLE_H_BASE * s;
    const float waveH = WAVE_H_BASE * s;
    const float descH = DESC_H_BASE * s;
    const float y = pad + titleH + waveH;
    const float contentW = screen.width - pad * 2.f;

    ui.text("beatDescription")
        .x(pad)
        .y(y)
        .size(contentW, descH)
        .text(getBeatDescription(ctx.beatFreq))
        .fontSize(18.f * s)
        .color({0.85f, 0.85f, 0.9f, 1.0f})
        .wrap()
        .build();
}

void composeControls(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx)
{
    const float s = ctx.uiScale;
    const float pad = PAD_BASE * s;
    const float titleH = TITLE_H_BASE * s;
    const float waveH = WAVE_H_BASE * s;
    const float descH = DESC_H_BASE * s;
    const float y = pad + titleH + waveH + descH;
    const float contentW = screen.width - pad * 2.f;
    const float rowH = ROW_H_BASE * s;

    // ── AI 驱动状态同步 ─────────────────────────────────────────────
    int curIdx = 0;
    if (!ctx.program.seq.empty()) {
        curIdx = ctx.synth.currentPeriodIndex();
        if (curIdx >= static_cast<int>(ctx.program.seq.size())) curIdx = 0;
    }
    if (ctx.playing && ctx.paramController.isAiDriven()) {
        ctx.beatFreq = ctx.paramController.currentBeatFreq();
    } else if (ctx.playing && !ctx.program.seq.empty() &&
               curIdx < static_cast<int>(ctx.program.seq.size())) {
        const binaural::Period* curP = ctx.synth.currentPeriod();
        if (curP && !curP->voices.empty()) {
            const auto& v = curP->voices[0];
            float len = static_cast<float>(curP->lengthSec);
            float pos = ctx.synth.periodElapsedSec();
            ctx.beatFreq =
                (len > 0.f) ? (v.freqStart + (v.freqEnd - v.freqStart) * (pos / len)) : v.freqStart;
            ctx.baseFreq = v.pitch > 0.f ? v.pitch : ctx.baseFreq;
        }
    } else if (!ctx.program.seq.empty() && curIdx < static_cast<int>(ctx.program.seq.size()) &&
               !ctx.program.seq[curIdx].voices.empty()) {
        ctx.beatFreq = ctx.program.seq[curIdx].voices[0].freqStart;
        ctx.baseFreq = ctx.program.seq[curIdx].voices[0].pitch > 0.f
                           ? ctx.program.seq[curIdx].voices[0].pitch
                           : ctx.baseFreq;
    }
    const bool aiDriven = ctx.paramController.isAiDriven();

    // ── 预计算 Background 选择（供 inline trigger + overlay dropdown 共用）──
    const char* kBgNames[] = {"No noise", "White noise", "Pink noise"};
    int bg = 0;

    // ── 控件（column 纵向排列，避免 stack 堆叠）────────────────────
    ui.column("controls.column")
        .x(pad)
        .y(y)
        .size(contentW, screen.height - y - pad)
        .gap(4.f * s)
        .content([&] {
            // Binaural Beat 滑块
            sliderWithButtons(ui, "ctrl.beatFreq", "Binaural Beat",
                              ctx.beatFreq, BEAT_MIN, BEAT_MAX, 0.5f,
                              "%.3f", "Hz", contentW, rowH, s,
                              [&ctx, curIdx](float) {
                                  if (!ctx.loadedFromGnaural)
                                      ctx.manualElapsedSec.store(0.f, std::memory_order_relaxed);
                                  if (!ctx.program.seq.empty() &&
                                      curIdx < static_cast<int>(ctx.program.seq.size()) &&
                                      !ctx.program.seq[curIdx].voices.empty()) {
                                      ctx.program.seq[curIdx].voices[0].freqStart = ctx.beatFreq;
                                      ctx.program.seq[curIdx].voices[0].freqEnd = ctx.beatFreq;
                                      ctx.synth.setProgram(ctx.program);
                                  }
                              },
                              nullptr, true);

            // 频率区间色条（在 Binaural Beat 滑块下方，左右对齐滑块轨道）
            {
                // 滑块轨道布局常量，与 euiComponents.h 中 sliderWithButtons 保持一致
                const float kBtnSz = 28.f * s;
                const float kGap = 6.f * s;
                const float kValW = 68.f * s;
                const float kLabelW = 130.f * s;
                const float trackOffset = kLabelW + kBtnSz + kGap * 2.f;
                const float trackW = std::max(40.f, contentW - kLabelW - kBtnSz * 2.f - kValW - kGap * 4.f);
                ui.stack("ctrl.bandsWrapper")
                    .size(contentW, 8.f * s)
                    .content([&] {
                        ui.stack("ctrl.bandsInner")
                            .x(trackOffset)
                            .y(0.f)
                            .size(trackW, 8.f * s)
                            .content([&] {
                                frequencyBandBar(ui, "ctrl.bands", ctx.beatFreq, trackW, 8.f * s, s);
                            })
                            .build();
                    })
                    .build();
            }

            // Base Frequency 滑块
            sliderWithButtons(ui, "ctrl.baseFreq", "Base Frequency",
                              ctx.baseFreq, BASE_FREQ_MIN, BASE_FREQ_MAX, 5.f,
                              "%.3f", "Hz", contentW, rowH, s,
                              [&ctx, curIdx](float) {
                                  if (!ctx.loadedFromGnaural)
                                      ctx.manualElapsedSec.store(0.f, std::memory_order_relaxed);
                                  if (!ctx.program.seq.empty() &&
                                      curIdx < static_cast<int>(ctx.program.seq.size()) &&
                                      !ctx.program.seq[curIdx].voices.empty()) {
                                      ctx.program.seq[curIdx].voices[0].pitch = ctx.baseFreq;
                                      ctx.synth.setProgram(ctx.program);
                                  }
                              },
                              nullptr, true);
            // Balance 滑块（右侧显示 Left/Center/Right）
            sliderWithButtons(ui, "ctrl.balance", "Balance",
                              ctx.balance, BALANCE_MIN, BALANCE_MAX, 0.1f,
                              "%.3f", "", contentW, rowH, s,
                              [&](float) {
                                  if (!ctx.loadedFromGnaural)
                                      ctx.manualElapsedSec.store(0.f, std::memory_order_relaxed);
                                  ctx.synth.setBalance(ctx.balance);
                              },
                              getBalanceLabel(ctx.balance));

            // ── 统一滑块宽度（Noise 和 Volume 共用右边缘，Play 按钮固定宽度避免 ▶→Stop 跳变）──
            const float volLabelW = 60.f * s;
            const float fixedPlayBtnW = 60.f * s;
            const float sliderGap = 8.f * s;
            const float volSliderW =
                std::max(40.f, contentW - volLabelW - fixedPlayBtnW - sliderGap * 2.f);

            // Isochronic + Background 行
            if (!ctx.program.seq.empty() && curIdx < static_cast<int>(ctx.program.seq.size()) &&
                !ctx.program.seq[curIdx].voices.empty()) {
                bool iso = ctx.program.seq[curIdx].voices[0].isochronic;
                bg = static_cast<int>(ctx.program.seq[curIdx].background);
                const float bgRowX = 120.f * s;
                const float dropdownW = std::max(120.f, 140.f * s);

                ui.row("ctrl.options")
                    .size(contentW, rowH)
                    .alignItems(core::Align::CENTER)
                    .gap(0.f)
                    .content([&] {
                        // Isochronic checkbox 区域（固定宽度 bgRowX）
                        ui.stack("ctrl.isochronicWrap")
                            .size(bgRowX, rowH)
                            .content([&] {
                                components::checkbox(ui, "ctrl.isochronic")
                                    .text("Isochronic")
                                    .checked(iso)
                                    .fontSize(18.f * s)
                                    .onChange([&ctx, curIdx](bool checked) {
                                        if (!ctx.loadedFromGnaural)
                                            ctx.manualElapsedSec.store(0.f,
                                                                       std::memory_order_relaxed);
                                        ctx.program.seq[curIdx].voices[0].isochronic = checked;
                                        ctx.synth.setProgram(ctx.program);
                                    })
                                    .build();
                            })
                            .build();

                        // Background 标签 + 触发按钮
                        ui.text("ctrl.background.label")
                            .size(96.f * s, rowH)
                            .text("Background")
                            .fontSize(18.f * s)
                            .color(components::theme::DarkThemeColors().text)
                            .verticalAlign(core::VerticalAlign::Center)
                            .build();

                        ui.stack("ctrl.background.triggerWrap")
                            .size(dropdownW, rowH - 2.f * s)
                            .content([&] {
                                components::button(ui, "ctrl.background.trigger")
                                    .size(dropdownW, rowH - 2.f * s)
                                    .text(kBgNames[bg])
                                    .fontSize(15.f * s)
                                    .secondaryTheme(components::theme::DarkThemeColors())
                                    .onClick([&] {
                                        g_bgDropdownOpen = !g_bgDropdownOpen;
                                    })
                                    .build();
                            })
                            .build();
                    })
                    .build();
            }

            // 噪声音量滑块（label/slider 左边缘与 Volume 行对齐，slider 延伸至 Volume Play 按钮右边缘）
            if (!ctx.program.seq.empty() && curIdx < static_cast<int>(ctx.program.seq.size()) &&
                !ctx.program.seq[curIdx].voices.empty() &&
                ctx.program.seq[curIdx].background != binaural::Period::Background::None) {
                float& noiseVol = ctx.program.seq[curIdx].backgroundVol;
                const float normalizedNoise = std::clamp(noiseVol, 0.f, 1.f);
                const float noiseSliderW = volSliderW;

                ui.row("ctrl.noise")
                    .size(contentW, rowH)
                    .alignItems(core::Align::CENTER)
                    .gap(sliderGap)
                    .content([&] {
                        ui.text("ctrl.noise.label")
                            .size(volLabelW, rowH)
                            .text("Noise")
                            .fontSize(18.f * s)
                            .color(components::theme::DarkThemeColors().text)
                            .verticalAlign(core::VerticalAlign::Center)
                            .build();

                        components::slider(ui, "ctrl.noise.slider")
                            .size(noiseSliderW, rowH)
                            .value(normalizedNoise)
                            .onChange([&](float v) {
                                noiseVol = v;
                                if (!ctx.loadedFromGnaural)
                                    ctx.manualElapsedSec.store(0.f,
                                                               std::memory_order_relaxed);
                                ctx.synth.setProgram(ctx.program);
                            })
                            .build();
                    })
                    .build();
            }

            // Volume + Play/Stop 行（Play 按钮固定宽度，避免 ▶→Stop 切换时滑块跳变）
            const float normalizedVol = std::clamp(ctx.volume / VOL_MAX, 0.f, 1.f);

            ui.row("ctrl.playRow")
                .size(contentW, rowH)
                .alignItems(core::Align::CENTER)
                .gap(sliderGap)
                .content([&] {
                    ui.text("ctrl.volume.label")
                        .size(volLabelW, rowH)
                        .text("Volume")
                        .fontSize(18.f * s)
                        .color(components::theme::DarkThemeColors().text)
                        .verticalAlign(core::VerticalAlign::Center)
                        .build();

                    components::slider(ui, "ctrl.volume.slider")
                        .size(volSliderW, rowH)
                        .value(normalizedVol)
                        .onChange([&](float v) {
                            ctx.volume = v * VOL_MAX;
                            ctx.synth.setVolumeMultiplier(ctx.volume);
                        })
                        .build();

                    components::button(ui, "ctrl.playBtn")
                        .size(fixedPlayBtnW, rowH)
                        .text(ctx.playing ? "Stop" : "\xE2\x96\xB6")  // ▶
                        .fontSize(17.f * s)
                        .radius(16.f * s)
                        .colors(
                            ctx.playing
                                ? core::Color{0.9f, 0.3f, 0.3f, 1.f}
                                : core::Color{0.25f, 0.55f, 0.95f, 1.f},
                            ctx.playing
                                ? core::Color{1.0f, 0.35f, 0.35f, 1.f}
                                : core::Color{0.3f, 0.6f, 1.0f, 1.f},
                            ctx.playing
                                ? core::Color{0.7f, 0.2f, 0.2f, 1.f}
                                : core::Color{0.2f, 0.45f, 0.8f, 1.f})
                        .onClick([&] {
                            if (!ctx.playing) {
                                PlaybackController::start(ctx);
                            } else {
                                PlaybackController::stop(ctx);
                            }
                        })
                        .build();
                })
                .build();
        })
        .build();

    // ── Background 下拉悬浮层（独立于 column，避免被后续子元素遮挡）──
    if (!ctx.program.seq.empty() && curIdx < static_cast<int>(ctx.program.seq.size()) &&
        !ctx.program.seq[curIdx].voices.empty()) {
        const float bgRowX = 120.f * s;
        const float dropdownW = std::max(120.f, 140.f * s);
        const float dropdownX = pad + bgRowX + 96.f * s;
        const float dropdownY = y + 3.f * rowH + 25.f * s; // options row top + field y-offset

        components::DropdownStyle bgDropdownStyle;
        bgDropdownStyle.popup = {0.16f, 0.18f, 0.22f, 0.98f};
        bgDropdownStyle.radius = 10.f;

        ui.stack("ctrl.background.dropdownOverlay")
            .x(dropdownX)
            .y(dropdownY)
            .size(dropdownW + 8.f * s, rowH + 140.f * s) // 足够容纳 popup
            .zIndex(500)
            .content([&] {
                components::dropdown(ui, "ctrl.background.overlay")
                    .size(dropdownW, rowH - 2.f * s)
                    .items({"No noise", "White noise", "Pink noise"})
                    .selected(static_cast<int>(ctx.program.seq[curIdx].background))
                    .open(g_bgDropdownOpen)
                    .fontSize(16.f * s)
                    .style(bgDropdownStyle)
                    .onOpenChange([](bool open) {
                        g_bgDropdownOpen = open;
                    })
                    .onChange([&ctx, curIdx](int sel) {
                        if (!ctx.loadedFromGnaural)
                            ctx.manualElapsedSec.store(0.f, std::memory_order_relaxed);
                        ctx.program.seq[curIdx].background =
                            static_cast<binaural::Period::Background>(sel);
                        ctx.synth.setProgram(ctx.program);
                        g_bgDropdownOpen = false;
                    })
                    .build();
            })
            .build();
    }
}

bool isBackgroundDropdownOpen()
{
    return g_bgDropdownOpen;
}

}  // namespace gui
