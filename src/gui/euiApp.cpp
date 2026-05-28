#include "app/dsl_app.h"

#include "components/components.h"

#include "binaural/audioDriver.hpp"
#include "binaural/parameterController.hpp"
#include "binaural/period.hpp"
#include "binaural/synthesizer.hpp"
#include "binaural/waveformBuffer.hpp"
#include "gui/guiPanels.hpp"
#include "gui/playbackController.hpp"

// 声明式页面 compose 函数
namespace gui {
void composeTitleBar(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx);
void composeWaveform(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx);
void composeBeatDescription(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx);
void composeControls(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx);
void composeHelpCenter(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx);
void composeLoadModal(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx);
void composeTimedPlaybackModal(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx);
void composeMenuPopup(core::dsl::Ui& ui, const core::dsl::Screen& screen, AppContext& ctx);
}  // namespace gui

namespace app {

namespace {

// ── 音频引擎 ──────────────────────────────────────────────────────
bool g_initialized = false;

binaural::Program                              g_program;
binaural::SynthesizerConfig                    g_config{44100, 2048};
std::unique_ptr<binaural::Synthesizer>         g_synth;
std::unique_ptr<binaural::ParameterController> g_paramCtrl;
binaural::ParameterController::PredictionQueue g_predQueue;
std::unique_ptr<binaural::WaveformBuffer>      g_waveBuf;
std::unique_ptr<binaural::IAudioDriver>        g_driver;

// ── 用户参数 ──────────────────────────────────────────────────────
float g_beatFreq = 4.f;
float g_baseFreq = 161.f;
float g_balance  = 0.f;
float g_volume   = 0.7f;

// ── 播放标志 ──────────────────────────────────────────────────────
bool               g_playing                  = false;
bool               g_loadedFromGnaural      = false;
bool               g_timedPlaybackEnabled     = false;
float              g_timedPlaybackDurationSec = 600.f;
std::atomic<float> g_manualElapsedSec{0.f};

// ── 弹窗 / 菜单 ───────────────────────────────────────────────────
bool g_showLoadModal          = false;
bool g_showHelpCenter         = false;
bool g_showTimedPlaybackModal = false;
bool g_showMenu               = false;
bool g_modalOpen              = false;
char g_loadPathBuf[512]       = {};

// ── UI 缩放 ──────────────────────────────────────────────────────
float g_uiScale = 1.f;

// ── 持久 AppContext（音频回调持有引用，禁止每帧栈分配）────────────
gui::AppContext* g_ctx = nullptr;

constexpr core::Color kBg{0.09f, 0.10f, 0.12f, 1.0f};

constexpr float kRefWidth  = 960.f;
constexpr float kRefHeight = 720.f;

void initAudioEngine()
{
    if (g_initialized) return;
    g_initialized = true;

    g_program.name = "Theta meditation";
    g_program.seq.push_back({
        .lengthSec = 3600,
        .voices    = {{
            .freqStart  = 4.0f,
            .freqEnd    = 4.0f,
            .volume     = 0.7f,
            .pitch      = 161.f,
            .isochronic = false,
        }},
        .background    = binaural::Period::Background::None,
        .backgroundVol = 0.f,
    });

    g_synth = std::make_unique<binaural::Synthesizer>(g_config);
    g_synth->setProgram(g_program);

    g_paramCtrl = std::make_unique<binaural::ParameterController>(*g_synth, g_predQueue);
    g_waveBuf   = std::make_unique<binaural::WaveformBuffer>(g_config.bufferFrames);
    g_driver    = binaural::createPortAudioDriver();

    static gui::AppContext ctx{
        .program                  = g_program,
        .synth                    = *g_synth,
        .paramController          = *g_paramCtrl,
        .predQueue                = g_predQueue,
        .waveBuf                  = *g_waveBuf,
        .config                   = g_config,
        .driver                   = g_driver.get(),
        .manualElapsedSec         = g_manualElapsedSec,
        .loadPathBuf              = g_loadPathBuf,
    };
    g_ctx = &ctx;
}

void syncFromAppContext()
{
    if (!g_ctx) return;
    g_beatFreq                 = g_ctx->beatFreq;
    g_baseFreq                 = g_ctx->baseFreq;
    g_balance                  = g_ctx->balance;
    g_volume                   = g_ctx->volume;
    g_playing                  = g_ctx->playing;
    g_showLoadModal            = g_ctx->showLoadModal;
    g_showHelpCenter           = g_ctx->showHelpCenter;
    g_loadedFromGnaural        = g_ctx->loadedFromGnaural;
    g_timedPlaybackEnabled     = g_ctx->timedPlaybackEnabled;
    g_timedPlaybackDurationSec = g_ctx->timedPlaybackDurationSec;
    g_showTimedPlaybackModal   = g_ctx->showTimedPlaybackModal;
    g_showMenu                 = g_ctx->showMenu;
    g_modalOpen                = g_ctx->modalOpen;
    g_uiScale                  = g_ctx->uiScale;
}

}  // namespace

bool appRequestsContinuousRender()
{
    return g_playing || g_showHelpCenter || g_showMenu || g_showLoadModal ||
           g_showTimedPlaybackModal || g_modalOpen || gui::isBackgroundDropdownOpen();
}

const DslAppConfig& dslAppConfig()
{
    static DslAppConfig config = DslAppConfig{}
        .title("Binaural Beats")
        .windowSize(960, 720)
        .clearColor(kBg)
        .fps(60.0)
        .iconFont("../font/JetBrainsMonoNerdFontMono-Regular.ttf");
    return config;
}

void compose(core::dsl::Ui& ui, const core::dsl::Screen& screen)
{
    initAudioEngine();
    // AppContext 为持久对象；onClick 修改 ctx 后 needsCompose 会立即 recompose。
    // 切勿在 compose 开头用 globals 覆盖 ctx，否则刚触发的 UI 状态会被清掉。

    gui::AppContext& ctx = *g_ctx;

    float uiScale = std::min(screen.width / kRefWidth, screen.height / kRefHeight);
    uiScale = std::clamp(uiScale, 0.5f, 2.0f);
    ctx.uiScale = uiScale;
    g_uiScale = uiScale;

    ctx.modalOpen = false;
    g_modalOpen = false;

    ui.stack("root")
        .size(screen.width, screen.height)
        .content([&] {
            gui::composeTitleBar(ui, screen, ctx);
            gui::composeWaveform(ui, screen, ctx);
            gui::composeBeatDescription(ui, screen, ctx);
            gui::composeControls(ui, screen, ctx);
        })
        .build();

    gui::composeHelpCenter(ui, screen, ctx);
    gui::composeLoadModal(ui, screen, ctx);
    gui::composeTimedPlaybackModal(ui, screen, ctx);
    gui::composeMenuPopup(ui, screen, ctx);

    if (ctx.playing && !ctx.loadedFromGnaural && ctx.timedPlaybackEnabled &&
        ctx.manualElapsedSec.load(std::memory_order_relaxed) >= ctx.timedPlaybackDurationSec) {
        gui::PlaybackController::stop(ctx);
    }

    syncFromAppContext();
}

}  // namespace app
