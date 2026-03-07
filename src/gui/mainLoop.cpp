#include "gui/mainLoop.hpp"
#include "gui/guiPanels.hpp"
#include "gui/guiUtils.hpp"
#include "binaural/parameterController.hpp"
#include "binaural/audioDriver.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <algorithm>

namespace gui {

void doOneRenderFrame(RenderFrameData &data) {
  if (!data.ctx || !data.window)
    return;
  AppContext &ctx = *data.ctx;
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  // DPI: backend sets DisplaySize/DisplayFramebufferScale from GLFW. Apply content scale for Windows 125% etc.
  ImGuiIO &io = ImGui::GetIO();
  float displayW = (std::max)(1.f, io.DisplaySize.x);
  float displayH = (std::max)(1.f, io.DisplaySize.y);

  float contentScale = 1.f;
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 3
  float scaleX, scaleY;
  glfwGetWindowContentScale(data.window, &scaleX, &scaleY);
  contentScale = (std::max)(scaleX, scaleY);
  if (contentScale < 0.5f)
    contentScale = 1.f;
#endif

  ImGui::NewFrame();

  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(displayW, displayH), ImGuiCond_Always);
  float uiScale = std::min(displayW / REF_WIDTH, displayH / REF_HEIGHT);
  uiScale = std::clamp(uiScale, 0.5f, 1.5f);
  // Dampen contentScale to avoid oversized UI (125%->1.06, 150%->1.12)
  float dampedContent = (contentScale > 1.f)
                           ? (1.f + (contentScale - 1.f) * 0.25f)
                           : contentScale;
  float effectiveScale = dampedContent * uiScale;
  ctx.uiScale = effectiveScale;
  ImGui::GetIO().FontGlobalScale = uiScale;
  applyScaledStyle(effectiveScale);

  ctx.modalOpen = false;
  if (ctx.showLoadModal) {
    ImGui::OpenPopup("Load Gnaural");
    ctx.showLoadModal = false;
  }
  if (ctx.showTimedPlaybackModal) {
    ImGui::OpenPopup("Timed Playback");
    ctx.showTimedPlaybackModal = false;
  }

  renderLoadModal(ctx);
  renderTimedPlaybackModal(ctx);

  ImGui::Begin("Main", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoCollapse);

  renderTitleBar(ctx);
  renderWaveform(ctx);
  renderBeatDescription(ctx);
  renderControls(ctx);

  ImGui::End();

  renderHelpCenter(ctx);

  if (data.paramController && data.driver &&
      ctx.playing && !ctx.loadedFromGnaural && ctx.timedPlaybackEnabled &&
      ctx.manualElapsedSec >= ctx.timedPlaybackDurationSec) {
    data.paramController->clearAiState();
    data.driver->stop();
    ctx.playing = false;
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

} // namespace gui
