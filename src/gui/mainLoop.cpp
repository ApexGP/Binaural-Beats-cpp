#include "gui/mainLoop.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>

#include "binaural/audioDriver.hpp"
#include "binaural/parameterController.hpp"
#include "gui/guiPanels.hpp"
#include "gui/guiUtils.hpp"
#include "gui/playbackController.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace gui {

void doOneRenderFrame(RenderFrameData &data)
{
    if (!data.ctx || !data.window) return;
    AppContext &ctx = *data.ctx;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    // DPI: backend sets DisplaySize/DisplayFramebufferScale from GLFW.
    ImGuiIO &io = ImGui::GetIO();
    float displayW = (std::max)(1.f, io.DisplaySize.x);
    float displayH = (std::max)(1.f, io.DisplaySize.y);

    ImGui::NewFrame();

    // uiScale: proportional to window size vs. reference resolution.
    // Font was baked at 16*dpiScale px; FontGlobalScale compensates for window size changes.
    // contentScale (DPI) is already baked into font size — do NOT double-apply here.
    float uiScale = (std::min)(displayW / REF_WIDTH, displayH / REF_HEIGHT);
    uiScale = std::clamp(uiScale, 0.5f, 2.0f);
    ctx.uiScale = uiScale;
    ImGui::GetIO().FontGlobalScale = uiScale;
    applyScaledStyle(uiScale);

    ctx.modalOpen = false;
    if (ctx.showLoadModal) {
        ImGui::OpenPopup("Load Gnaural");
        ctx.showLoadModal = false;
    }
    if (ctx.showTimedPlaybackModal) {
        ImGui::OpenPopup("Timed Playback");
        ctx.showTimedPlaybackModal = false;
    }

    // Modals first: BeginPopupModal calls ClearFlags() when popup is closed,
    // which would wipe SetNextWindowPos/Size if called before Begin("Main").
    renderLoadModal(ctx);
    renderTimedPlaybackModal(ctx);

    // Set pos/size after modal calls so ClearFlags() cannot consume them.
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(displayW, displayH), ImGuiCond_Always);
    ImGui::Begin("Main", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);

    renderTitleBar(ctx);
    renderWaveform(ctx);
    renderBeatDescription(ctx);
    renderControls(ctx);

    ImGui::End();

    renderHelpCenter(ctx);

    // Debug overlay — remove once resize behaviour is confirmed correct.
    {
        int gw = 0, gh = 0, fw = 0, fh = 0;
        glfwGetWindowSize(data.window, &gw, &gh);
        glfwGetFramebufferSize(data.window, &fw, &fh);
        // pivot (0,1) anchors the bottom-left corner to this point, so the
        // overlay always sits fully inside the window regardless of its height.
        ImGui::SetNextWindowPos(ImVec2(4.f, displayH - 4.f), ImGuiCond_Always, ImVec2(0.f, 1.f));
        ImGui::SetNextWindowBgAlpha(0.65f);
        ImGui::Begin("##dbg", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
        ImGui::TextDisabled("glfw:%dx%d  io:%.0fx%.0f  fb:%dx%d  scale:%.2f", gw, gh,
                            io.DisplaySize.x, io.DisplaySize.y, fw, fh, uiScale);
        ImGui::End();
    }

    if (data.paramController && ctx.playing && !ctx.loadedFromGnaural && ctx.timedPlaybackEnabled &&
        ctx.manualElapsedSec.load(std::memory_order_relaxed) >= ctx.timedPlaybackDurationSec) {
        PlaybackController::stop(ctx);
    }

    ImGui::Render();
    int fbW, fbH;
    glfwGetFramebufferSize(data.window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);
    glClearColor(0.12f, 0.12f, 0.14f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(data.window);
}

}  // namespace gui
