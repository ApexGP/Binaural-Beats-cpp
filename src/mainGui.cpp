#include "binaural/audioDriver.hpp"
#include "binaural/gnauralParser.hpp"
#include "binaural/period.hpp"
#include "binaural/synthesizer.hpp"
#include "binaural/wavDriver.hpp"
#include "binaural/waveformBuffer.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
// clang-format off: windows.h must precede commdlg.h
#include <windows.h>
#include <commdlg.h>
// clang-format on
#endif

using namespace binaural;

namespace {
constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;
constexpr float BEAT_MIN = 0.5f;
constexpr float BEAT_MAX = 40.f;
constexpr float BASE_FREQ_MIN = 40.f;
constexpr float BASE_FREQ_MAX = 500.f;
constexpr float BALANCE_MIN = -1.f;
constexpr float BALANCE_MAX = 1.f;
constexpr float VOL_MIN = 0.f;
constexpr float VOL_MAX = 1.2f;

const char *getBeatDescription(float hz) {
  if (hz < 4.f)
    return "Delta waves are associated with deep sleep cycles and the release "
           "of physical awareness.";
  if (hz < 8.f)
    return "Theta waves are linked to meditation, creativity, and light sleep.";
  if (hz < 12.f)
    return "Alpha waves are associated with relaxed wakefulness and calm "
           "focus.";
  if (hz < 30.f)
    return "Beta waves are associated with active thinking, focus, and "
           "alertness.";
  return "Gamma waves are associated with higher cognition and peak "
         "concentration.";
}

const char *getBalanceLabel(float b) {
  if (b < -0.3f)
    return "Left";
  if (b > 0.3f)
    return "Right";
  return "Center";
}

void applyDarkTheme() {
  ImGui::StyleColorsDark();
  ImVec4 *colors = ImGui::GetStyle().Colors;
  const ImVec4 grayBg(0.12f, 0.12f, 0.14f, 1.00f);
  colors[ImGuiCol_WindowBg] = grayBg;
  colors[ImGuiCol_ChildBg] = grayBg;
  colors[ImGuiCol_Separator] = grayBg;
  colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.28f, 0.30f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.35f, 0.38f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 1.00f, 1.00f, 0.80f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.38f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.45f, 0.55f, 0.75f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.20f, 0.45f, 0.80f, 0.60f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.50f, 0.85f, 0.80f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.55f, 0.90f, 1.00f);
  colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
  ImGui::GetStyle().FrameRounding = 12.f;
  ImGui::GetStyle().GrabRounding = 12.f;
  ImGui::GetStyle().WindowRounding = 0.f;
  ImGui::GetStyle().PopupRounding = 12.f;
  ImGui::GetStyle().WindowBorderSize = 0.f;
  ImGui::GetStyle().ChildBorderSize = 0.f;
  ImGui::GetStyle().WindowPadding = ImVec2(16.f, 12.f);
}

bool sliderWithButtons(const char *label, float *v, float minV, float maxV,
                       const char *fmt, float step, const char *valueLabel,
                       const char *inputFmt = nullptr,
                       const char *unitSuffix = nullptr) {
  bool changed = false;
  ImGui::PushID(label);
  ImGui::BeginGroup();
  ImGui::AlignTextToFramePadding();
  ImGui::Text("%s", label);
  float regionX = ImGui::GetContentRegionAvail().x;
  ImGui::SameLine(regionX - 78);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
  if (inputFmt && inputFmt[0]) {
    ImGui::SetNextItemWidth(58);
    float tmp = *v;
    if (ImGui::InputFloat("##val", &tmp, 0, 0, inputFmt,
                          ImGuiInputTextFlags_EnterReturnsTrue |
                              ImGuiInputTextFlags_CharsDecimal)) {
      *v = (std::max)(minV, (std::min)(maxV, tmp));
      changed = true;
    }
    if (unitSuffix && unitSuffix[0]) {
      ImGui::SameLine(0, 2);
      ImGui::Text("%s", unitSuffix);
    }
  } else {
    ImGui::Text("%s", valueLabel);
  }
  ImGui::PopStyleColor();
  ImGui::Spacing();
  const float btnReserve = 72.f;
  float avail = ImGui::GetContentRegionAvail().x - btnReserve;
  if (avail < 0)
    avail = 0;
  ImGui::PushButtonRepeat(true);
  if (ImGui::Button("-", ImVec2(28, 28))) {
    *v = (std::max)(minV, *v - step);
    changed = true;
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(avail);
  if (ImGui::SliderFloat("##slider", v, minV, maxV, nullptr,
                         ImGuiSliderFlags_NoInput)) {
    changed = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("+", ImVec2(28, 28))) {
    *v = (std::min)(maxV, *v + step);
    changed = true;
  }
  ImGui::PopButtonRepeat();
  ImGui::EndGroup();
  ImGui::PopID();
  return changed;
}

std::string getFontDir() {
  namespace fs = std::filesystem;
  std::string fontDir;
#ifdef _WIN32
  wchar_t exePath[MAX_PATH];
  if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
    fs::path p(exePath);
    for (int i = 0; i < 3; ++i) {
      auto dir = p.parent_path() / "font";
      if (fs::exists(dir))
        return dir.string();
      p = p.parent_path();
    }
  }
#endif
  for (const char *rel : {"font", "../font", "../../font"}) {
    if (fs::exists(rel))
      return rel;
  }
  return "";
}

void loadFontsFromDir() {
  namespace fs = std::filesystem;
  ImGuiIO &io = ImGui::GetIO();
  std::string fontDir = getFontDir();
  if (fontDir.empty()) {
    io.Fonts->AddFontDefault();
    return;
  }
  const float fontSize = 16.0f;
  static const ImWchar *iconRanges = nullptr;
  static ImVector<ImWchar> iconRangesBuf;
  {
    ImFontGlyphRangesBuilder b;
    b.AddRanges(io.Fonts->GetGlyphRangesDefault());
    b.AddChar(0x23F1);
    b.AddChar(0x22EE);
    b.AddChar(0x25B6);
    b.AddChar(0xEB7C);
    b.AddChar(0xEB10);
    b.AddChar(0xEB2C);
    b.BuildRanges(&iconRangesBuf);
    iconRanges = iconRangesBuf.Data;
  }
  const char *defaultOrder[] = {
      "NotoSans-SemiBold.ttf",
      "NotoSansSymbols-VariableFont_wght.ttf",
      "NotoSansSymbols-SemiBold.ttf",
      "NotoSansSymbols2-Regular.ttf",
  };
  ImFont *baseFont = nullptr;
  for (const char *name : defaultOrder) {
    std::string path = fontDir + "/" + name;
    if (fs::exists(path)) {
      baseFont = io.Fonts->AddFontFromFileTTF(path.c_str(), fontSize, nullptr,
                                              iconRanges);
      if (baseFont)
        break;
    }
  }
  if (!baseFont) {
    for (const auto &e : fs::directory_iterator(fontDir)) {
      if (e.path().extension() == ".ttf" || e.path().extension() == ".otf") {
        baseFont = io.Fonts->AddFontFromFileTTF(e.path().string().c_str(),
                                                fontSize, nullptr, iconRanges);
        if (baseFont)
          break;
      }
    }
  }
  if (baseFont) {
    ImFontConfig cfg;
    cfg.MergeMode = true;
    std::string basePath;
    for (const char *name : defaultOrder) {
      std::string path = fontDir + "/" + name;
      if (fs::exists(path)) {
        basePath = path;
        break;
      }
    }
    if (basePath.empty()) {
      for (const auto &e : fs::directory_iterator(fontDir)) {
        if (e.path().extension() == ".ttf" || e.path().extension() == ".otf") {
          basePath = e.path().string();
          break;
        }
      }
    }
    for (const auto &e : fs::directory_iterator(fontDir)) {
      std::string p = e.path().string();
      if ((e.path().extension() == ".ttf" || e.path().extension() == ".otf") &&
          p != basePath) {
        io.Fonts->AddFontFromFileTTF(p.c_str(), fontSize, &cfg, iconRanges);
      }
    }
    io.FontDefault = baseFont;
  } else {
    io.Fonts->AddFontDefault();
  }
}
} // namespace

int main() {
  Program program;
  program.name = "Alpha Relax";
  program.seq.push_back({
      .lengthSec = 3600,
      .voices = {{
          .freqStart = 10.0f,
          .freqEnd = 10.0f,
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

  WaveformBuffer waveBuf(2048);

  float beatFreq = 10.f;
  float baseFreq = 161.f;
  float balance = 0.f;
  float volume = 0.7f;
  bool playing = false;
  bool showLoadModal = false;
  char loadPathBuf[512] = "";

  auto driver = createPortAudioDriver();
  if (dynamic_cast<WavFileDriver *>(driver.get())) {
    return 1;
  }

  if (!glfwInit()) {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window =
      glfwCreateWindow(WIDTH, HEIGHT, "Binaural Beats", nullptr, nullptr);
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
  loadFontsFromDir();

  applyDarkTheme();

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

    const float pad = 20.f;
    const float titleH = 48.f;
    const float waveH = 140.f;
    const float descH = 44.f;
    const float ctrlH = 36.f;
    const float ctrlGap = 8.f;

    ImGui::SetCursorPos(ImVec2(pad, pad));
    ImGui::BeginChild("TitleBar", ImVec2(-1, titleH), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::TextDisabled("\u23F1"); // stopwatch
    ImGui::SameLine(ImGui::GetWindowWidth() / 2.f - 60);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4);
    ImGui::Text("Binaural Beats");
    ImGui::SameLine(ImGui::GetWindowWidth() - 40);
    if (ImGui::Button("\u22EE", ImVec2(24, 24))) {
      ImGui::OpenPopup("MenuPopup");
    }
    if (ImGui::BeginPopup("MenuPopup")) {
      if (ImGuiWindow *w = ImGui::GetCurrentWindow())
        w->WindowRounding = 12.f;
      if (ImGui::MenuItem("Load Gnaural...")) {
        loadPathBuf[0] = '\0';
        showLoadModal = true;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
    ImGui::EndChild();

    ImGui::SetCursorPos(ImVec2(pad, pad + titleH));
    ImGui::BeginChild("Waveform", ImVec2(-1, waveH), ImGuiChildFlags_None);
    ImGui::BeginChild("WaveArea", ImVec2(-1, -1), ImGuiChildFlags_None);

    std::vector<float> samplesL, samplesR;
    waveBuf.getSamples(samplesL, samplesR);
    if (!samplesL.empty() && playing) {
      float h = (waveH - 24) / 2.f;
      ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.95f, 0.5f, 0.2f, 1.f));
      ImGui::PlotLines("##L", samplesL.data(),
                       static_cast<int>(samplesL.size()), 0, nullptr, -1.f, 1.f,
                       ImVec2(-1, h));
      ImGui::PlotLines("##R", samplesR.data(),
                       static_cast<int>(samplesR.size()), 0, nullptr, -1.f, 1.f,
                       ImVec2(-1, h));
      ImGui::PopStyleColor();
    } else {
      ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionAvail().x / 2.f - 40,
                                 (waveH - 24) / 2.f - 8));
      ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.45f, 1.f), "Waveform");
    }
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::SetCursorPos(ImVec2(pad, pad + titleH + waveH));
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x +
                           ImGui::GetCursorScreenPos().x);
    ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.9f, 1.f), "%s",
                       getBeatDescription(beatFreq));
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    ImGui::SetCursorPos(ImVec2(pad, pad + titleH + waveH + descH));
    ImGui::BeginChild("Controls", ImVec2(-1, -1), ImGuiChildFlags_None);

    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f Hz", beatFreq);
    if (sliderWithButtons("Binaural Beat", &beatFreq, BEAT_MIN, BEAT_MAX,
                          "%.1f Hz", 0.5f, buf, "%.1f", "Hz")) {
      if (!program.seq.empty() && !program.seq[0].voices.empty()) {
        program.seq[0].voices[0].freqStart = beatFreq;
        program.seq[0].voices[0].freqEnd = beatFreq;
        synth.setProgram(program);
      }
    }
    constexpr float BAND_RANGES[] = {3.5f, 4.f, 4.f, 18.f, 10.f};
    constexpr float BAND_TOTAL = 39.5f;
    const float bandAvail = ImGui::GetContentRegionAvail().x - 72.f;
    if (bandAvail > 0) {
      ImGui::SetCursorPosX(28);
      const ImVec4 bandColors[] = {
          ImVec4(0.9f, 0.25f, 0.2f, 1.f),  ImVec4(0.95f, 0.5f, 0.2f, 1.f),
          ImVec4(0.95f, 0.85f, 0.2f, 1.f), ImVec4(0.3f, 0.75f, 0.35f, 1.f),
          ImVec4(0.6f, 0.35f, 0.85f, 1.f),
      };
      const float gap = 1.f;
      const float totalBandW = bandAvail - 4.f * gap;
      for (int i = 0; i < 5; ++i) {
        if (i > 0)
          ImGui::SameLine(0, gap);
        const float w = (BAND_RANGES[i] / BAND_TOTAL) * totalBandW;
        if (w >= 2.f) {
          ImGui::PushID(i);
          ImGui::PushStyleColor(ImGuiCol_Button, bandColors[i]);
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bandColors[i]);
          ImGui::PushStyleColor(ImGuiCol_ButtonActive, bandColors[i]);
          ImGui::Button("##band", ImVec2(w, 8));
          ImGui::PopStyleColor(3);
          ImGui::PopID();
        }
      }
    }
    ImGui::Spacing();

    snprintf(buf, sizeof(buf), "%.0f Hz", baseFreq);
    if (sliderWithButtons("Base Frequency", &baseFreq, BASE_FREQ_MIN,
                          BASE_FREQ_MAX, "%.0f Hz", 5.f, buf, "%.0f", "Hz")) {
      if (!program.seq.empty() && !program.seq[0].voices.empty()) {
        program.seq[0].voices[0].pitch = baseFreq;
        synth.setProgram(program);
      }
    }
    ImGui::Spacing();

    if (sliderWithButtons("Balance", &balance, BALANCE_MIN, BALANCE_MAX, "%.2f",
                          0.1f, getBalanceLabel(balance), "%.2f", "")) {
      synth.setBalance(balance);
    }
    ImGui::Spacing();

    if (!program.seq.empty() && !program.seq[0].voices.empty()) {
      bool iso = program.seq[0].voices[0].isochronic;
      if (ImGui::Checkbox("Isochronic", &iso)) {
        program.seq[0].voices[0].isochronic = iso;
        synth.setProgram(program);
      }
      ImGui::SameLine(120);
      int bg = static_cast<int>(program.seq[0].background);
      const char *bgNames[] = {"No noise", "Pink noise", "White noise"};
      if (ImGui::Combo("Background", &bg, bgNames, 3)) {
        program.seq[0].background = static_cast<Period::Background>(bg);
        synth.setProgram(program);
      }
      if (program.seq[0].background != Period::Background::None) {
        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Noise");
        ImGui::SameLine(70);
        float noiseW = ImGui::GetContentRegionAvail().x - 20.f;
        if (noiseW < 80.f)
          noiseW = 80.f;
        ImGui::PushID("NoiseVol");
        ImGui::SetNextItemWidth(noiseW);
        if (ImGui::SliderFloat("##noise", &program.seq[0].backgroundVol, 0.f,
                               0.5f, "%.2f")) {
          synth.setProgram(program);
        }
        ImGui::PopID();
      }
    }
    ImGui::Spacing();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Volume");
    ImGui::SameLine(70);
    const float btnW = playing ? 60.f : 40.f;
    const float reserve = btnW + 40.f;
    float volW = ImGui::GetContentRegionAvail().x - reserve;
    if (volW < 0)
      volW = 0;
    ImGui::SetNextItemWidth(volW);
    if (ImGui::SliderFloat("##vol", &volume, VOL_MIN, VOL_MAX, "%.2f")) {
      synth.setVolumeMultiplier(volume);
    }
    ImGui::SameLine();
    const char *playLabel = playing ? "Stop" : "\u25B6"; // play
    if (ImGui::Button(playLabel, ImVec2(btnW, 32))) {
      playing = !playing;
      if (playing) {
        driver->start(config.sampleRate, config.bufferFrames,
                      [&](std::vector<int16_t> &buf) {
                        synth.skewVoices(synth.periodElapsedSec());
                        synth.fillSamples(buf);
                        for (size_t i = 0; i < buf.size(); i += 8) {
                          float l = buf[i] / 32768.f;
                          float r = buf[i + 1] / 32768.f;
                          waveBuf.push(l, r);
                        }
                        synth.advanceTime(
                            static_cast<float>(config.bufferFrames) /
                            config.sampleRate);
                      });
      } else {
        driver->stop();
      }
    }
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(12, 0));

    ImGui::EndChild();
    ImGui::End();

    if (showLoadModal) {
      ImGui::OpenPopup("Load Gnaural");
      showLoadModal = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
    if (ImGui::BeginPopupModal("Load Gnaural", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("File path (.txt or .gnaural):");
      ImGui::SetNextItemWidth(400);
      ImGui::InputText("##path", loadPathBuf, sizeof(loadPathBuf));
#ifdef _WIN32
      if (ImGui::Button("Browse...")) {
        wchar_t pathW[512] = {};
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter =
            L"Gnaural (*.txt;*.gnaural)\0*.txt;*.gnaural\0All (*.*)\0*.*\0";
        ofn.lpstrFile = pathW;
        ofn.nMaxFile = 512;
        ofn.Flags = OFN_FILEMUSTEXIST;
        if (GetOpenFileNameW(&ofn)) {
          char pathA[512];
          WideCharToMultiByte(CP_UTF8, 0, pathW, -1, pathA, 512, 0, 0);
          strncpy(loadPathBuf, pathA, sizeof(loadPathBuf) - 1);
          loadPathBuf[sizeof(loadPathBuf) - 1] = '\0';
        }
      }
      ImGui::SameLine();
#endif
      if (ImGui::Button("Load")) {
        if (auto prog = parseGnaural(loadPathBuf)) {
          program = std::move(*prog);
          synth.setProgram(program);
          if (!program.seq.empty() && !program.seq[0].voices.empty()) {
            beatFreq = program.seq[0].voices[0].freqStart;
            baseFreq = program.seq[0].voices[0].pitch > 0
                           ? program.seq[0].voices[0].pitch
                           : 161.f;
          }
          ImGui::CloseCurrentPopup();
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
    ImGui::PopStyleVar(3);

    ImGui::Render();
    int displayW, displayH;
    glfwGetFramebufferSize(window, &displayW, &displayH);
    glViewport(0, 0, displayW, displayH);
    glClearColor(0.12f, 0.12f, 0.14f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  if (playing)
    driver->stop();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
