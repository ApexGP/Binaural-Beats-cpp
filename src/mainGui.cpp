#include "binaural/audioDriver.hpp"
#include "binaural/parameterController.hpp"
#include "binaural/period.hpp"
#include "binaural/synthesizer.hpp"
#include "binaural/wavDriver.hpp"
#include "binaural/waveformBuffer.hpp"
#include "gui/guiFonts.hpp"
#include "gui/guiPanels.hpp"
#include "gui/guiUtils.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <memory>

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
  };

  auto driver = createPortAudioDriver();
  if (dynamic_cast<WavFileDriver *>(driver.get())) {
    return 1;
  }
  ctx.driver = driver.get();

  if (!glfwInit()) {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(gui::WIDTH, gui::HEIGHT, "Binaural Beats",
                                        nullptr, nullptr);
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

  io.Fonts->Clear();
  gui::loadFontsFromDir();
  gui::applyDarkTheme();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    int winW, winH;
    glfwGetFramebufferSize(window, &winW, &winH);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(
        ImVec2(static_cast<float>(winW), static_cast<float>(winH)));
    ImGui::Begin("Main", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse);

    gui::renderTitleBar(ctx);
    gui::renderWaveform(ctx);
    gui::renderBeatDescription(ctx);
    gui::renderControls(ctx);

    ImGui::End();

    gui::renderHelpCenter(ctx);
    gui::renderLoadModal(ctx);

    ImGui::Render();
    int displayW, displayH;
    glfwGetFramebufferSize(window, &displayW, &displayH);
    glViewport(0, 0, displayW, displayH);
    glClearColor(0.12f, 0.12f, 0.14f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
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
