// euiModals.cpp — 弹窗 compose 函数
// 从 guiPanels.cpp 的 ImGui 模态渲染函数迁移到 EUI-NEO 声明式 DSL

#include "gui/guiPanels.hpp"
#include "gui/guiUtils.hpp"
#include "gui/playbackController.hpp"
#include "gui/euiComponents.h"

#include "binaural/eegPredictorInterface.hpp"
#include "binaural/period.hpp"

#ifdef _WIN32
// clang-format off: windows.h must precede commdlg.h
#include <windows.h>
#include <commdlg.h>
// clang-format on
#endif

#include <algorithm>
#include <cstdio>
#include <vector>

namespace gui {

namespace {

// ── 主题色 ─────────────────────────────────────────────────────────
constexpr core::Color kTextPrimary {0.94f, 0.97f, 1.00f, 1.0f};
constexpr core::Color kAccent      {0.25f, 0.55f, 0.95f, 1.0f};
constexpr core::Color kAccentDim   {0.40f, 0.70f, 1.00f, 1.0f};

}  // namespace

void composeMenuPopup(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx)
{
    if (!ctx.showMenu) return;

    const float s = ctx.uiScale;
    const float pad = 20.f * s;
    const float menuBtnReserve = 44.f * s;
    const float contentW = screen.width - pad * 2.f;
    const float menuX = pad + contentW - menuBtnReserve;
    const float menuY = pad + 28.f * s;  // 低于 ⋮ 按钮

    // 动态菜单项（与 ImGui 版逻辑一致）
    std::vector<std::string> items;
    if (ctx.timedPlaybackEnabled && !ctx.loadedFromGnaural &&
        !ctx.paramController.isAiDriven()) {
        items = {"Exit timed playback"};
    } else if (ctx.loadedFromGnaural || ctx.paramController.isAiDriven()) {
        items = {"Return to manual control"};
    } else {
        items = {"Load Gnaural...", "Timed playback...", "Simulate AI (push 12 Hz)"};
    }

    components::contextMenu(ui, "menuPopup")
        .open(true)
        .screen(screen.width, screen.height)
        .position(menuX, menuY)
        .size(220.f * s, 32.f * s)
        .fontSize(15.f * s)
        .zIndex(200)
        .items(items)
        .onSelect([&](int idx) {
            if (ctx.timedPlaybackEnabled && !ctx.loadedFromGnaural &&
                !ctx.paramController.isAiDriven()) {
                // 唯一项：Exit timed playback
                PlaybackController::exitTimedPlayback(ctx);
            } else if (ctx.loadedFromGnaural || ctx.paramController.isAiDriven()) {
                // 唯一项：Return to manual control
                PlaybackController::returnToManual(ctx);
            } else {
                switch (idx) {
                case 0:  // Load Gnaural...
                    ctx.loadPathBuf[0] = '\0';
                    ctx.showLoadModal = true;
                    break;
                case 1:  // Timed playback...
                    ctx.showTimedPlaybackModal = true;
                    break;
                case 2:  // Simulate AI
                    {
                        binaural::EEGStatePrediction p;
                        p.state = binaural::EEGStatePrediction::State::Relaxed;
                        p.targetBeatFreq = 12.f;
                        p.confidence = 0.9f;
                        ctx.predQueue.push(p);
                    }
                    break;
                }
            }
            ctx.showMenu = false;
        })
        .onDismiss([&] { ctx.showMenu = false; })
        .build();
}

void composeHelpCenter(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx)
{
    if (!ctx.showHelpCenter) return;

    const float s = ctx.uiScale;
    const float modalW = std::min(520.f * s, screen.width * 0.85f);
    const float modalH = std::min(480.f * s, screen.height * 0.82f);
    const float px = (screen.width - modalW) * 0.5f;
    const float py = (screen.height - modalH) * 0.5f;
    const float pad = 16.f * s;
    const float titleH = 28.f * s;
    const float closeBtnH = 30.f * s;
    const float scrollW = modalW - pad * 2.f;
    const float scrollH = modalH - pad * 2.f - titleH - closeBtnH - 12.f * s;

    // 跨帧保持滚动偏移
    static float g_helpScrollOffset = 0.f;

    beginCustomModal(ui, "helpCenter", screen.width, screen.height, modalW, modalH);
    ctx.modalOpen = true;

    // 内容容器定位在 modal panel 上
    ui.stack("helpCenter.content")
        .x(px).y(py).size(modalW, modalH)
        .z(102)
        .content([&] {
            // 标题
            ui.text("helpCenter.title")
                .x(pad).y(pad)
                .size(modalW - pad * 2.f, titleH)
                .text("Help Center")
                .fontSize(21.f * s)
                .color(kTextPrimary)
                .horizontalAlign(core::HorizontalAlign::Left)
                .verticalAlign(core::VerticalAlign::Center)
                .build();

            // 可滚动内容
            const float scrollY = pad + titleH + 4.f * s;
            ui.stack("helpCenter.scrollContainer")
                .x(pad).y(scrollY).size(scrollW, scrollH)
                .clip()
                .content([&] {
                    components::scrollView(ui, "helpCenter.scroll")
                        .size(scrollW, scrollH)
                        .offset(g_helpScrollOffset)
                        .gap(6.f * s)
                        .step(24.f * s)
                        .onChange([](float v) { g_helpScrollOffset = v; })
                        .content([&](core::dsl::Ui& svUi, float cw, float) {
                            // ── Parameters ──
                            svUi.text("help.params.header")
                                .width(cw)
                                .text("Parameters")
                                .fontSize(20.f * s)
                                .color(kAccentDim)
                                .build();

                            svUi.text("help.params.binaural")
                                .width(cw)
                                .text("Binaural Beat: Frequency difference between left and right carriers, producing brainwave entrainment. Ranging from 0 to 40 Hz.")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();

                            svUi.text("help.params.bands")
                                .width(cw)
                                .text("Bands: Delta(0.5-4), Theta(4-8), Alpha(8-13), Beta(13-30), Gamma(30-40).")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();

                            svUi.text("help.params.baseFreq")
                                .width(cw)
                                .text("Base Frequency: Carrier center frequency, typically ranging from 40 to 500 Hz.")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();

                            svUi.text("help.params.baseFreq2")
                                .width(cw)
                                .text("Common: 161 Hz or 200 Hz. Too high may cause discomfort.")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();

                            svUi.text("help.params.balance")
                                .width(cw)
                                .text("Balance: -1 for full left, 0 for center, and 1 for full right. Adjusts stereo volume ratio.")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();

                            svUi.text("help.params.volume")
                                .width(cw)
                                .text("Volume: Master output level.")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();

                            svUi.text("help.params.isochronic")
                                .width(cw)
                                .text("Isochronic: Adds pulsed beats when enabled, works without headphones.")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();

                            svUi.text("help.params.background")
                                .width(cw)
                                .text("Background: Pink noise, white noise, etc., useful for meditation.")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();

                            // ── Example Presets ──
                            svUi.text("help.presets.header")
                                .width(cw)
                                .text("Example Presets")
                                .fontSize(20.f * s)
                                .color(kAccentDim)
                                .build();

                            svUi.text("help.presets.meditation")
                                .width(cw)
                                .text("Meditation/Relax: 4 Hz(Theta), base 161 Hz, 20 to 40 minutes.")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();

                            svUi.text("help.presets.focus")
                                .width(cw)
                                .text("Focus/Study: 12 Hz(Alpha) or 14 Hz(Beta), base 200 Hz.")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();

                            svUi.text("help.presets.sleep")
                                .width(cw)
                                .text("Deep sleep: 2 Hz(Delta), low volume, with pink noise.")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();

                            svUi.text("help.presets.creativity")
                                .width(cw)
                                .text("Creativity/Light sleep: 6 Hz(Theta), enable isochronic.")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();

                            svUi.text("help.presets.gnaural")
                                .width(cw)
                                .text("You can also try to load from a Gnaural file in the menu to get started.")
                                .fontSize(18.f * s)
                                .color(kTextPrimary)
                                .wrap()
                                .build();
                        })
                        .build();
                })
                .build();

            // Close 按钮
            ui.stack("helpCenter.closeWrapper")
                .x(modalW - 100.f * s - pad)
                .y(modalH - closeBtnH - pad)
                .size(100.f * s, closeBtnH)
                .content([&] {
                    components::button(ui, "helpCenter.close")
                        .size(100.f * s, closeBtnH)
                        .text("Close")
                        .fontSize(16.f * s)
                        .radius(8.f * s)
                        .onClick([&] { ctx.showHelpCenter = false; })
                        .build();
                })
                .build();
        })
        .build();
}

void composeLoadModal(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx)
{
    if (!ctx.showLoadModal) return;

    const float s = ctx.uiScale;
    const float modalW = 480.f * s;
    const float modalH = 180.f * s;
    const float px = (screen.width - modalW) * 0.5f;
    const float py = (screen.height - modalH) * 0.5f;
    const float pad = 16.f * s;
    const float rowH = 30.f * s;
    const float btnW = 80.f * s;
    const float inputW = modalW - pad * 2.f;

    beginCustomModal(ui, "loadModal", screen.width, screen.height, modalW, modalH);
    ctx.modalOpen = true;

    ui.stack("loadModal.content")
        .x(px).y(py).size(modalW, modalH)
        .z(102)
        .content([&] {
            // 标题
            ui.text("loadModal.title")
                .x(pad).y(pad)
                .size(inputW, rowH)
                .text("File path (.txt or .gnaural):")
                .fontSize(18.f * s)
                .color(kTextPrimary)
                .verticalAlign(core::VerticalAlign::Center)
                .build();

            // 文件路径输入
            ui.stack("loadModal.pathWrapper")
                .x(pad).y(pad + rowH + 8.f * s)
                .size(inputW, rowH)
                .content([&] {
                    components::input(ui, "loadModal.path")
                        .size(inputW, rowH)
                        .value(std::string(ctx.loadPathBuf))
                        .fontSize(16.f * s)
                        .onChange([&](const std::string& v) {
                            std::snprintf(ctx.loadPathBuf, sizeof(ctx.loadPathBuf), "%s", v.c_str());
                        })
                        .build();
                })
                .build();

            const float btnY = pad + rowH + 8.f * s + rowH + 16.f * s;
#ifdef _WIN32
            ui.stack("loadModal.browseWrapper")
                .x(pad).y(btnY)
                .size(btnW, rowH)
                .content([&] {
                    components::button(ui, "loadModal.browse")
                        .size(btnW, rowH)
                        .text("Browse...")
                        .fontSize(15.f * s)
                        .radius(8.f * s)
                        .onClick([&] {
                            wchar_t pathW[512] = {};
                            OPENFILENAMEW ofn = {};
                            ofn.lStructSize = sizeof(ofn);
                            ofn.lpstrFilter =
                                L"Gnaural (*.txt;*.gnaural)\0*.txt;*.gnaural\0All (*.*)\0*.*\0";
                            ofn.lpstrFile = pathW;
                            ofn.nMaxFile = 512;
                            ofn.Flags = OFN_FILEMUSTEXIST;
                            if (GetOpenFileNameW(&ofn)) {
                                int needed = WideCharToMultiByte(CP_UTF8, 0, pathW, -1, nullptr, 0,
                                                                 nullptr, nullptr);
                                if (needed > 0) {
                                    std::vector<char> pathA(static_cast<size_t>(needed));
                                    WideCharToMultiByte(CP_UTF8, 0, pathW, -1, pathA.data(), needed,
                                                        nullptr, nullptr);
                                    std::snprintf(ctx.loadPathBuf, sizeof(ctx.loadPathBuf), "%s",
                                                  pathA.data());
                                }
                            }
                        })
                        .build();
                })
                .build();

            ui.stack("loadModal.loadWrapper")
                .x(pad + btnW + 8.f * s).y(btnY)
                .size(btnW, rowH)
                .content([&] {
                    components::button(ui, "loadModal.load")
                        .size(btnW, rowH)
                        .text("Load")
                        .fontSize(15.f * s)
                        .radius(8.f * s)
                        .colors(kAccent,
                                core::Color{0.3f, 0.6f, 1.0f, 1.f},
                                core::Color{0.2f, 0.45f, 0.8f, 1.f})
                        .onClick([&] {
                            if (PlaybackController::loadGnaural(ctx)) {
                                ctx.showLoadModal = false;
                            }
                        })
                        .build();
                })
                .build();

            ui.stack("loadModal.cancelWrapper")
                .x(pad + btnW * 2.f + 16.f * s).y(btnY)
                .size(btnW, rowH)
                .content([&] {
                    components::button(ui, "loadModal.cancel")
                        .size(btnW, rowH)
                        .text("Cancel")
                        .fontSize(15.f * s)
                        .radius(8.f * s)
                        .onClick([&] { ctx.showLoadModal = false; })
                        .build();
                })
                .build();
#else
            ui.stack("loadModal.loadWrapper")
                .x(pad).y(btnY)
                .size(btnW, rowH)
                .content([&] {
                    components::button(ui, "loadModal.load")
                        .size(btnW, rowH)
                        .text("Load")
                        .fontSize(15.f * s)
                        .radius(8.f * s)
                        .colors(kAccent,
                                core::Color{0.3f, 0.6f, 1.0f, 1.f},
                                core::Color{0.2f, 0.45f, 0.8f, 1.f})
                        .onClick([&] {
                            if (PlaybackController::loadGnaural(ctx)) {
                                ctx.showLoadModal = false;
                            }
                        })
                        .build();
                })
                .build();

            ui.stack("loadModal.cancelWrapper")
                .x(pad + btnW + 8.f * s).y(btnY)
                .size(btnW, rowH)
                .content([&] {
                    components::button(ui, "loadModal.cancel")
                        .size(btnW, rowH)
                        .text("Cancel")
                        .fontSize(15.f * s)
                        .radius(8.f * s)
                        .onClick([&] { ctx.showLoadModal = false; })
                        .build();
                })
                .build();
#endif
        })
        .build();
}

void composeTimedPlaybackModal(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx)
{
    if (!ctx.showTimedPlaybackModal) return;

    const float s = ctx.uiScale;
    const float modalW = 320.f * s;
    const float modalH = 170.f * s;
    const float px = (screen.width - modalW) * 0.5f;
    const float py = (screen.height - modalH) * 0.5f;
    const float pad = 16.f * s;
    const float rowH = 30.f * s;
    const float btnW = 80.f * s;
    const float inputW = 120.f * s;

    // 仅在 modal 刚打开时初始化输入缓冲（避免每帧覆盖用户输入）
    static std::string g_timedInputStr;
    static bool g_timedWasOpen = false;
    if (!g_timedWasOpen) {
        g_timedInputStr = formatFloat(ctx.timedPlaybackDurationSec, "%.0f");
        g_timedWasOpen = true;
    }

    beginCustomModal(ui, "timedModal", screen.width, screen.height, modalW, modalH);
    ctx.modalOpen = true;

    ui.stack("timedModal.content")
        .x(px).y(py).size(modalW, modalH)
        .z(102)
        .content([&] {
            // 标题
            ui.text("timedModal.title")
                .x(pad).y(pad)
                .size(modalW - pad * 2.f, rowH)
                .text("Set playback duration (seconds):")
                .fontSize(18.f * s)
                .color(kTextPrimary)
                .verticalAlign(core::VerticalAlign::Center)
                .build();

            // 时长输入
            ui.stack("timedModal.inputWrapper")
                .x(pad).y(pad + rowH + 8.f * s)
                .size(inputW, rowH)
                .content([&] {
                    components::input(ui, "timedModal.duration")
                        .size(inputW, rowH)
                        .value(g_timedInputStr)
                        .fontSize(20.f * s)
                        .onChange([&](const std::string& v) { g_timedInputStr = v; })
                        .build();
                })
                .build();

            // 按钮
            const float btnY = pad + rowH + 8.f * s + rowH + 16.f * s;
            ui.stack("timedModal.okWrapper")
                .x(pad).y(btnY)
                .size(btnW, rowH)
                .content([&] {
                    components::button(ui, "timedModal.ok")
                        .size(btnW, rowH)
                        .text("OK")
                        .fontSize(15.f * s)
                        .radius(8.f * s)
                        .colors(kAccent,
                                core::Color{0.3f, 0.6f, 1.0f, 1.f},
                                core::Color{0.2f, 0.45f, 0.8f, 1.f})
                        .onClick([&] {
                            float dur = parseFloat(g_timedInputStr, 600.f);
                            if (dur < 1.f) dur = 1.f;
                            ctx.timedPlaybackDurationSec = dur;
                            ctx.timedPlaybackEnabled = true;
                            ctx.manualElapsedSec.store(0.f, std::memory_order_relaxed);
                            ctx.showTimedPlaybackModal = false;
                            g_timedWasOpen = false;
                        })
                        .build();
                })
                .build();

            ui.stack("timedModal.cancelWrapper")
                .x(pad + btnW + 8.f * s).y(btnY)
                .size(btnW, rowH)
                .content([&] {
                    components::button(ui, "timedModal.cancel")
                        .size(btnW, rowH)
                        .text("Cancel")
                        .fontSize(15.f * s)
                        .radius(8.f * s)
                        .onClick([&] {
                            ctx.showTimedPlaybackModal = false;
                            g_timedWasOpen = false;
                        })
                        .build();
                })
                .build();
        })
        .build();
}

}  // namespace gui
