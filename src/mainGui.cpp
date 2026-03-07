#include <GLFW/glfw3.h>

#include <memory>

#include "binaural/audioDriver.hpp"
#include "binaural/parameterController.hpp"
#include "binaural/period.hpp"
#include "binaural/synthesizer.hpp"
#include "binaural/wavDriver.hpp"
#include "binaural/waveformBuffer.hpp"
#include "gui/guiFonts.hpp"
#include "gui/guiPanels.hpp"
#include "gui/guiUtils.hpp"
#include "gui/mainLoop.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"


#ifdef _WIN32
#include <windows.h>
#endif

using namespace binaural;

int main() {
  Program program;
  program.name = "Theta meditation";
  program.seq.push_back({
      .lengthSec = 3600,
      .voices = {{
          .freqStart = 4.0f,
          .freqEnd = 4.0f,
          .volume = 0.7f,
          .pitch = 161.f,
          .isochronic = false,
      }},
      .background = Period::Background::None,
      .backgroundVol = 0.f,
  });

  SynthesizerConfig config;
  config.sampleRate = 44100;
  config.bufferFrames = 2048;
  config.iscale = 1440;

  Synthesizer synth(config);
  synth.setProgram(program);

  ParameterController::PredictionQueue predQueue;
  ParameterController paramController(synth, predQueue);

  WaveformBuffer waveBuf(2048);

  gui::AppContext ctx{
      .program = program,
      .synth = synth,
      .paramController = paramController,
      .predQueue = predQueue,
      .waveBuf = waveBuf,
      .config = config,
      .driver = nullptr,
      .beatFreq = 4.f,
      .baseFreq = 161.f,
      .balance = 0.f,
      .volume = 0.7f,
      .playing = false,
      .showLoadModal = false,
      .showHelpCenter = false,
      .loadedFromGnaural = false,
      .manualElapsedSec = 0.f,
      .timedPlaybackEnabled = false,
      .timedPlaybackDurationSec = 600.f,
      .showTimedPlaybackModal = false,
      .modalOpen = false,
  };

  auto driver = createPortAudioDriver();
  if (dynamic_cast<WavFileDriver *>(driver.get())) {
    return 1;
  }
  ctx.driver = driver.get();

#ifdef _WIN32
  // DPI awareness before glfwInit for correct window size on high-DPI displays
  if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
    using SetDpiCtxFn = BOOL(WINAPI *)(void *);
    auto setDpi = reinterpret_cast<SetDpiCtxFn>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setDpi)
      setDpi(reinterpret_cast<void *>(
          -4));  // DPI_AWARENESS_CONTEXT_PER_MONITOR_V2
  }
#endif

  if (!glfwInit()) {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  // TODO: Re-enable when resize scaling is fixed. Win32 modal resize blocks
  // main loop, causing stale DisplaySize.
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  GLFWwindow *window = glfwCreateWindow(gui::WIDTH, gui::HEIGHT,
                                        "Binaural Beats", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  float dpiScale = 1.f;
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 3
  float sx, sy;
  glfwGetWindowContentScale(window, &sx, &sy);
  dpiScale = (sx > sy) ? sx : sy;
  if (dpiScale < 0.5f)
    dpiScale = 1.f;
#endif
  io.Fonts->Clear();
  gui::loadFontsFromDir(dpiScale);
  gui::applyDarkTheme();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  gui::RenderFrameData frameData{&ctx, window, &paramController, driver.get()};

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    gui::doOneRenderFrame(frameData);
  }

  if (ctx.playing) {
    paramController.clearAiState();
    driver->stop();
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
