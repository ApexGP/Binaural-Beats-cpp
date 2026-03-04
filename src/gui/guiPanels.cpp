#ifdef _WIN32
// clang-format off: windows.h must precede commdlg.h
#include <windows.h>
#include <commdlg.h>
// clang-format on
#endif

#include "gui/guiPanels.hpp"
#include "gui/guiUtils.hpp"
#include "binaural/eegPredictorInterface.hpp"
#include "binaural/gnauralParser.hpp"
#include "binaural/period.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cstdio>
#include <vector>

namespace gui {

namespace {
constexpr float PAD = 20.f;
constexpr float TITLE_H = 48.f;
constexpr float WAVE_H = 140.f;
constexpr float DESC_H = 44.f;
} // namespace

void renderTitleBar(AppContext &ctx) {
  ImGui::SetCursorPos(ImVec2(PAD, PAD));
  ImGui::BeginChild("TitleBar", ImVec2(-1, TITLE_H), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar);
  if (ImGui::Button("\u2753", ImVec2(24, 24))) {
    ctx.showHelpCenter = true;
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Help Center");
  ImGui::SameLine(ImGui::GetWindowWidth() / 2.f - 60);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4);
  ImGui::Text("Binaural Beats");
  char eBuf[16], tBuf[16];
  static float frozenManualElapsed = 0.f;
  static bool modalWasOpen = false;
  if (ctx.modalOpen) {
    if (!modalWasOpen)
      frozenManualElapsed = ctx.manualElapsedSec;
    modalWasOpen = true;
  } else {
    modalWasOpen = false;
  }
  const bool modalOpen = ctx.modalOpen;
  if (ctx.loadedFromGnaural && !ctx.program.seq.empty()) {
    int idx = ctx.synth.currentPeriodIndex();
    if (idx >= static_cast<int>(ctx.program.seq.size()))
      idx = 0;
    snprintf(eBuf, sizeof(eBuf), "%d",
             static_cast<int>(ctx.synth.periodElapsedSec() + 0.5f));
    snprintf(tBuf, sizeof(tBuf), "%d", ctx.program.seq[idx].lengthSec);
  } else {
    const float elapsedForDisplay =
        modalOpen ? frozenManualElapsed : ctx.manualElapsedSec;
    snprintf(eBuf, sizeof(eBuf), "%d",
             static_cast<int>(elapsedForDisplay + 0.5f));
    if (ctx.timedPlaybackEnabled) {
      snprintf(tBuf, sizeof(tBuf), "%d",
               static_cast<int>(ctx.timedPlaybackDurationSec + 0.5f));
    } else {
      snprintf(tBuf, sizeof(tBuf), "\u221E");
    }
  }
  const float menuBtnW = 24.f;
  const float menuBtnReserve = 44.f;
  const float durBlockW = 210.f;
  const float titleBarW = ImGui::GetWindowWidth();
  ImGui::SameLine(titleBarW - menuBtnReserve - durBlockW);
  ImGui::BeginGroup();
  ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.f, 1.f), "duration");
  ImGui::SameLine();
  if (ctx.loadedFromGnaural && !ctx.program.seq.empty()) {
    int idx = ctx.synth.currentPeriodIndex();
    if (idx >= static_cast<int>(ctx.program.seq.size()))
      idx = 0;
    float elapsed = ctx.synth.periodElapsedSec();
    float totalSec = static_cast<float>(ctx.program.seq[idx].lengthSec);
    static float durationEditBuf = 0.f;
    ImGui::PushID("durElapsed");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.85f, 0.4f, 1.f));
    ImGui::SetNextItemWidth(56);
    if (ImGui::InputFloat("##dur", &durationEditBuf, 0, 0, "%.0f",
                          ImGuiInputTextFlags_EnterReturnsTrue |
                              ImGuiInputTextFlags_CharsDecimal)) {
      durationEditBuf = std::clamp(durationEditBuf, 0.f, totalSec);
      ctx.synth.setPeriodElapsedSec(durationEditBuf);
    }
    if (!ImGui::IsItemActive())
      durationEditBuf = elapsed;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 2);
    ImGui::TextDisabled("s");
    ImGui::PopID();
  } else {
    ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.4f, 1.f), "%s", eBuf);
    ImGui::SameLine(0, 2);
    ImGui::TextDisabled("s");
  }
  ImGui::SameLine();
  ImGui::TextDisabled(
      (ctx.loadedFromGnaural || ctx.timedPlaybackEnabled) ? " / %s s" : " / %s",
      tBuf);
  ImGui::EndGroup();
  ImGui::SameLine(titleBarW - menuBtnReserve);
  if (ImGui::Button("\u22EE", ImVec2(menuBtnW, 24))) {
    ImGui::OpenPopup("MenuPopup");
  }
  if (ImGui::BeginPopup("MenuPopup")) {
    if (ImGuiWindow *w = ImGui::GetCurrentWindow())
      w->WindowRounding = 12.f;
    if (ctx.timedPlaybackEnabled && !ctx.loadedFromGnaural &&
        !ctx.paramController.isAiDriven()) {
      if (ImGui::MenuItem("Exit timed playback")) {
        ctx.timedPlaybackEnabled = false;
        ctx.manualElapsedSec = 0.f;
        ImGui::CloseCurrentPopup();
      }
    } else if (ctx.loadedFromGnaural || ctx.paramController.isAiDriven()) {
      if (ImGui::MenuItem("Return to manual control")) {
        const bool wasAiDriven = ctx.paramController.isAiDriven();
        if (wasAiDriven) {
          ctx.beatFreq = ctx.paramController.currentBeatFreq();
          int idx = ctx.synth.currentPeriodIndex();
          if (!ctx.program.seq.empty() &&
              idx < static_cast<int>(ctx.program.seq.size()) &&
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
        ctx.manualElapsedSec = 0.f;
        ImGui::CloseCurrentPopup();
      }
    } else {
      if (ImGui::MenuItem("Load Gnaural...")) {
        ctx.loadPathBuf[0] = '\0';
        ctx.showLoadModal = true;
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::MenuItem("Timed playback...")) {
        ctx.showTimedPlaybackModal = true;
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::MenuItem("Simulate AI (push 12 Hz)")) {
        binaural::EEGStatePrediction p;
        p.state = binaural::EEGStatePrediction::State::Relaxed;
        p.targetBeatFreq = 12.f;
        p.confidence = 0.9f;
        ctx.predQueue.push(p);
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }
  ImGui::EndChild();
}

void renderWaveform(AppContext &ctx) {
  ImGui::SetCursorPos(ImVec2(PAD, PAD + TITLE_H));
  ImGui::BeginChild("Waveform", ImVec2(-1, WAVE_H), ImGuiChildFlags_None);
  ImGui::BeginChild("WaveArea", ImVec2(-1, -1), ImGuiChildFlags_None);
  std::vector<float> samplesL, samplesR;
  ctx.waveBuf.getSamples(samplesL, samplesR);
  if (!samplesL.empty()) {
    float h = (WAVE_H - 24) / 2.f;
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
                               (WAVE_H - 24) / 2.f - 8));
    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.45f, 1.f), "Waveform");
  }
  ImGui::EndChild();
  ImGui::EndChild();
}

void renderBeatDescription(AppContext &ctx) {
  ImGui::SetCursorPos(ImVec2(PAD, PAD + TITLE_H + WAVE_H));
  ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x +
                         ImGui::GetCursorScreenPos().x);
  ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.9f, 1.f), "%s",
                     getBeatDescription(ctx.beatFreq));
  ImGui::PopTextWrapPos();
  ImGui::Spacing();
}

void renderControls(AppContext &ctx) {
  ImGui::SetCursorPos(ImVec2(PAD, PAD + TITLE_H + WAVE_H + DESC_H));
  ImGui::BeginChild("Controls", ImVec2(-1, -1), ImGuiChildFlags_None);

  int curIdx = 0;
  if (!ctx.program.seq.empty()) {
    curIdx = ctx.synth.currentPeriodIndex();
    if (curIdx >= static_cast<int>(ctx.program.seq.size()))
      curIdx = 0;
  }
  if (ctx.playing && ctx.paramController.isAiDriven()) {
    ctx.beatFreq = ctx.paramController.currentBeatFreq();
  } else if (ctx.playing && !ctx.program.seq.empty() &&
             curIdx < static_cast<int>(ctx.program.seq.size())) {
    const binaural::Period *curP = ctx.synth.currentPeriod();
    if (curP && !curP->voices.empty()) {
      const auto &v = curP->voices[0];
      float len = static_cast<float>(curP->lengthSec);
      float pos = ctx.synth.periodElapsedSec();
      ctx.beatFreq =
          (len > 0.f)
              ? (v.freqStart + (v.freqEnd - v.freqStart) * (pos / len))
              : v.freqStart;
      ctx.baseFreq = v.pitch > 0.f ? v.pitch : ctx.baseFreq;
    }
  } else if (!ctx.program.seq.empty() &&
             curIdx < static_cast<int>(ctx.program.seq.size()) &&
             !ctx.program.seq[curIdx].voices.empty()) {
    ctx.beatFreq = ctx.program.seq[curIdx].voices[0].freqStart;
    ctx.baseFreq = ctx.program.seq[curIdx].voices[0].pitch > 0.f
                       ? ctx.program.seq[curIdx].voices[0].pitch
                       : ctx.baseFreq;
  }
  if (ctx.paramController.isAiDriven()) {
    ImGui::BeginDisabled();
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%.3f Hz", ctx.beatFreq);
  if (sliderWithButtons("Binaural Beat", &ctx.beatFreq, BEAT_MIN, BEAT_MAX,
                        "%.3f Hz", 0.5f, buf, "%.3f", "Hz")) {
    if (!ctx.loadedFromGnaural)
      ctx.manualElapsedSec = 0.f;
    if (!ctx.program.seq.empty() &&
        curIdx < static_cast<int>(ctx.program.seq.size()) &&
        !ctx.program.seq[curIdx].voices.empty()) {
      ctx.program.seq[curIdx].voices[0].freqStart = ctx.beatFreq;
      ctx.program.seq[curIdx].voices[0].freqEnd = ctx.beatFreq;
      ctx.synth.setProgram(ctx.program);
    }
  }
  if (ctx.paramController.isAiDriven()) {
    ImGui::EndDisabled();
  }
  // Delta(0-4), Theta(4-8), Alpha(8-12), Beta(12-30), Gamma(30-40), aligned with getBeatDescription
  constexpr float BAND_RANGES[] = {4.f, 4.f, 4.f, 18.f, 10.f};
  constexpr float BAND_TOTAL = 40.f;
  const float bandAvail = ImGui::GetContentRegionAvail().x - 72.f;
  if (bandAvail > 0) {
    const float gap = 1.f;
    const float totalBandW = bandAvail - 4.f * gap;
    const float leftPad = (0.5f / BAND_TOTAL) * totalBandW;
    const float actualBandW = totalBandW - leftPad;
    ImGui::SetCursorPosX(28 + leftPad);
    const ImVec4 bandColors[] = {
        ImVec4(0.9f, 0.25f, 0.2f, 1.f),  ImVec4(0.95f, 0.5f, 0.2f, 1.f),
        ImVec4(0.95f, 0.85f, 0.2f, 1.f), ImVec4(0.3f, 0.75f, 0.35f, 1.f),
        ImVec4(0.6f, 0.35f, 0.85f, 1.f),
    };
    for (int i = 0; i < 5; ++i) {
      if (i > 0)
        ImGui::SameLine(0, gap);
      const float w = (BAND_RANGES[i] / BAND_TOTAL) * actualBandW;
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

  snprintf(buf, sizeof(buf), "%.3f Hz", ctx.baseFreq);
  if (sliderWithButtons("Base Frequency", &ctx.baseFreq, BASE_FREQ_MIN,
                        BASE_FREQ_MAX, "%.3f Hz", 5.f, buf, "%.3f", "Hz")) {
    if (!ctx.loadedFromGnaural)
      ctx.manualElapsedSec = 0.f;
    if (!ctx.program.seq.empty() &&
        curIdx < static_cast<int>(ctx.program.seq.size()) &&
        !ctx.program.seq[curIdx].voices.empty()) {
      ctx.program.seq[curIdx].voices[0].pitch = ctx.baseFreq;
      ctx.synth.setProgram(ctx.program);
    }
  }
  ImGui::Spacing();

  if (sliderWithButtons("Balance", &ctx.balance, BALANCE_MIN, BALANCE_MAX,
                        "%.3f", 0.1f, getBalanceLabel(ctx.balance), "%.3f",
                        "")) {
    if (!ctx.loadedFromGnaural)
      ctx.manualElapsedSec = 0.f;
    ctx.synth.setBalance(ctx.balance);
  }
  ImGui::Spacing();

  if (!ctx.program.seq.empty() &&
      curIdx < static_cast<int>(ctx.program.seq.size()) &&
      !ctx.program.seq[curIdx].voices.empty()) {
    bool iso = ctx.program.seq[curIdx].voices[0].isochronic;
    if (ImGui::Checkbox("Isochronic", &iso)) {
      if (!ctx.loadedFromGnaural)
        ctx.manualElapsedSec = 0.f;
      ctx.program.seq[curIdx].voices[0].isochronic = iso;
      ctx.synth.setProgram(ctx.program);
    }
    ImGui::SameLine(120);
    int bg = static_cast<int>(ctx.program.seq[curIdx].background);
    const char *bgNames[] = {"No noise", "Pink noise", "White noise"};
    if (ImGui::Combo("Background", &bg, bgNames, 3)) {
      if (!ctx.loadedFromGnaural)
        ctx.manualElapsedSec = 0.f;
      ctx.program.seq[curIdx].background =
          static_cast<binaural::Period::Background>(bg);
      ctx.synth.setProgram(ctx.program);
    }
    if (ctx.program.seq[curIdx].background !=
        binaural::Period::Background::None) {
      ImGui::Spacing();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Noise");
      ImGui::SameLine(70);
      float noiseW = ImGui::GetContentRegionAvail().x - 20.f;
      if (noiseW < 80.f)
        noiseW = 80.f;
      ImGui::PushID("NoiseVol");
      ImGui::SetNextItemWidth(noiseW);
      if (ImGui::SliderFloat("##noise",
                             &ctx.program.seq[curIdx].backgroundVol, 0.f, 1.f,
                             "%.2f")) {
        if (!ctx.loadedFromGnaural)
          ctx.manualElapsedSec = 0.f;
        ctx.synth.setProgram(ctx.program);
      }
      ImGui::PopID();
    }
  }
  ImGui::Spacing();

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Volume");
  ImGui::SameLine(70);
  const float btnW = ctx.playing ? 60.f : 40.f;
  const float reserve = btnW + 40.f;
  float volW = ImGui::GetContentRegionAvail().x - reserve;
  if (volW < 0)
    volW = 0;
  ImGui::SetNextItemWidth(volW);
  if (ImGui::SliderFloat("##vol", &ctx.volume, VOL_MIN, VOL_MAX, "%.2f")) {
    ctx.synth.setVolumeMultiplier(ctx.volume);
  }
  ImGui::SameLine();
  const char *playLabel = ctx.playing ? "Stop" : "\u25B6";
  if (ImGui::Button(playLabel, ImVec2(btnW, 32))) {
    ctx.playing = !ctx.playing;
    if (ctx.playing) {
      ctx.driver->start(
          ctx.config.sampleRate, ctx.config.bufferFrames,
          [&ctx](std::vector<int16_t> &buf) {
            float delta = static_cast<float>(ctx.config.bufferFrames) /
                          ctx.config.sampleRate;
            ctx.paramController.update(ctx.synth.periodElapsedSec());
            ctx.synth.fillSamples(buf);
            for (size_t i = 0; i < buf.size(); i += 8) {
              float l = buf[i] / 32768.f;
              float r = buf[i + 1] / 32768.f;
              ctx.waveBuf.push(l, r);
            }
            ctx.synth.advanceTime(delta);
            if (!ctx.loadedFromGnaural)
              ctx.manualElapsedSec += delta;
          });
    } else {
      ctx.paramController.clearAiState();
      ctx.driver->stop();
    }
  }
  ImGui::SameLine();
  ImGui::Dummy(ImVec2(12, 0));

  ImGui::EndChild();
}

void renderHelpCenter(AppContext &ctx) {
  if (!ctx.showHelpCenter)
    return;
  ImGui::SetNextWindowSize(ImVec2(480, 420), ImGuiCond_FirstUseEver);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
  if (ImGui::Begin("Help Center", &ctx.showHelpCenter,
                   ImGuiWindowFlags_NoCollapse)) {
    if (ImGui::BeginChild("HelpContent", ImVec2(0, -30),
                          ImGuiChildFlags_Border)) {
      ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.f, 1.f), "Parameters");
      ImGui::Separator();
      ImGui::TextWrapped(
          "Binaural Beat: Frequency difference between left and right "
          "carriers, producing brainwave entrainment. Ranging from 0 to "
          "40 Hz. ");
      ImGui::TextWrapped("Bands: Delta(0.5-4), Theta(4-8), Alpha(8-12), "
                         "Beta(12-30), Gamma(30-40).");
      ImGui::Spacing();
      ImGui::TextWrapped(
          "Base Frequency: Carrier center frequency, typically ranging "
          "from 40 to 500 Hz. ");
      ImGui::TextWrapped(
          "Common: 161 Hz or 200 Hz. Too high may cause discomfort.");
      ImGui::Spacing();
      ImGui::TextWrapped(
          "Balance: -1 for full left, 0 for center, and 1 for full right. "
          "Adjusts stereo volume ratio.");
      ImGui::Spacing();
      ImGui::TextWrapped("Volume: Master output level.");
      ImGui::TextWrapped("Isochronic: Adds pulsed beats when enabled, "
                         "works without headphones.");
      ImGui::TextWrapped("Background: Pink noise, white noise, etc., "
                         "useful for meditation.");
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.f, 1.f), "Example Presets");
      ImGui::Separator();
      ImGui::TextWrapped(
          "Meditation/Relax: 4 Hz(Theta), base 161 Hz, 20 to 40 minutes.");
      ImGui::TextWrapped(
          "Focus/Study: 12 Hz(Alpha) or 14 Hz(Beta), base 200 Hz.");
      ImGui::TextWrapped(
          "Deep sleep: 2 Hz(Delta), low volume, with pink noise.");
      ImGui::TextWrapped(
          "Creativity/Light sleep: 6 Hz(Theta), enable isochronic.");
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.f, 1.f),
                         "You can also try to load from a Gnaural file in "
                         "the menu to get started.");
      ImGui::EndChild();
    }
    if (ImGui::Button("Close", ImVec2(80, 0)))
      ctx.showHelpCenter = false;
    ImGui::End();
  }
  ImGui::PopStyleVar();
}

void renderLoadModal(AppContext &ctx) {
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
  ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 12.f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
  if (ImGui::BeginPopupModal("Load Gnaural", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ctx.modalOpen = true;
    ImGui::Text("File path (.txt or .gnaural):");
    ImGui::SetNextItemWidth(400);
    ImGui::InputText("##path", ctx.loadPathBuf, sizeof(ctx.loadPathBuf));
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
        strncpy(ctx.loadPathBuf, pathA, sizeof(ctx.loadPathBuf) - 1);
        ctx.loadPathBuf[sizeof(ctx.loadPathBuf) - 1] = '\0';
      }
    }
    ImGui::SameLine();
#endif
    if (ImGui::Button("Load")) {
      if (auto prog = binaural::parseGnaural(ctx.loadPathBuf)) {
        ctx.program = std::move(*prog);
        ctx.synth.setProgram(ctx.program);
        ctx.loadedFromGnaural = true;
        if (!ctx.program.seq.empty() &&
            !ctx.program.seq[0].voices.empty()) {
          ctx.beatFreq = ctx.program.seq[0].voices[0].freqStart;
          ctx.baseFreq = ctx.program.seq[0].voices[0].pitch > 0
                             ? ctx.program.seq[0].voices[0].pitch
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
}

void renderTimedPlaybackModal(AppContext &ctx) {
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Appearing);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
  ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 12.f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
  if (ImGui::BeginPopupModal("Timed Playback", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ctx.modalOpen = true;
    ImGui::Text("Set playback duration (seconds):");
    ImGui::SetNextItemWidth(120);
    ImGui::InputFloat("##duration", &ctx.timedPlaybackDurationSec, 60.f, 300.f,
                      "%.0f", ImGuiInputTextFlags_CharsDecimal);
    if (ctx.timedPlaybackDurationSec < 1.f)
      ctx.timedPlaybackDurationSec = 1.f;
    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(80, 0))) {
      ctx.timedPlaybackEnabled = true;
      ctx.manualElapsedSec = 0.f;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::PopStyleVar(3);
}

} // namespace gui
