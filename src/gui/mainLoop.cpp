#include "gui/mainLoop.hpp"
#include "gui/guiPanels.hpp"
#include "binaural/parameterController.hpp"
#include "binaural/audioDriver.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#endif

namespace gui {

void doOneRenderFrame(RenderFrameData &data) {
  if (!data.ctx || !data.window)
    return;
  AppContext &ctx = *data.ctx;
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  int winW, winH;
  glfwGetFramebufferSize(data.window, &winW, &winH);
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(
      ImVec2(static_cast<float>(winW), static_cast<float>(winH)));
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
  renderLoadModal(ctx);
  renderTimedPlaybackModal(ctx);

  if (data.paramController && data.driver &&
      ctx.playing && !ctx.loadedFromGnaural && ctx.timedPlaybackEnabled &&
      ctx.manualElapsedSec >= ctx.timedPlaybackDurationSec) {
    data.paramController->clearAiState();
    data.driver->stop();
    ctx.playing = false;
  }

  ImGui::Render();
  int displayW, displayH;
  glfwGetFramebufferSize(data.window, &displayW, &displayH);
  glViewport(0, 0, displayW, displayH);
  glClearColor(0.12f, 0.12f, 0.14f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(data.window);
}

#ifdef _WIN32
namespace {
constexpr UINT_PTR DRAG_RENDER_TIMER_ID = 1;
constexpr UINT DRAG_RENDER_TIMER_MS = 16;

static RenderFrameData g_renderData;
static WNDPROC g_originalWndProc = nullptr;

LRESULT CALLBACK subclassedWndProc(HWND hwnd, UINT msg, WPARAM wp,
                                   LPARAM lp) {
  if (msg == WM_ENTERSIZEMOVE) {
    SetTimer(hwnd, DRAG_RENDER_TIMER_ID, DRAG_RENDER_TIMER_MS, nullptr);
  } else if (msg == WM_EXITSIZEMOVE) {
    KillTimer(hwnd, DRAG_RENDER_TIMER_ID);
  } else if (msg == WM_TIMER && wp == DRAG_RENDER_TIMER_ID) {
    if (!glfwWindowShouldClose(g_renderData.window))
      doOneRenderFrame(g_renderData);
    return 0;
  }
  return CallWindowProcW(g_originalWndProc, hwnd, msg, wp, lp);
}
} // namespace

void installDragRenderSubclass(GLFWwindow *window,
                               const RenderFrameData &data) {
  g_renderData = data;
  HWND hwnd = glfwGetWin32Window(window);
  if (!hwnd)
    return;
  g_originalWndProc = reinterpret_cast<WNDPROC>(
      SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                       reinterpret_cast<LONG_PTR>(subclassedWndProc)));
}
#else
void installDragRenderSubclass(GLFWwindow *, const RenderFrameData &) {}
#endif

} // namespace gui
