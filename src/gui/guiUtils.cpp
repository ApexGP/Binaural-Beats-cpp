#include "gui/guiUtils.hpp"
#include <algorithm>

namespace gui {

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
                       const char *inputFmt, const char *unitSuffix) {
  bool changed = false;
  ImGui::PushID(label);
  ImGui::BeginGroup();
  ImGui::AlignTextToFramePadding();
  ImGui::Text("%s", label);
  float regionX = ImGui::GetContentRegionAvail().x;
  ImGui::SameLine(regionX - 88);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
  if (inputFmt && inputFmt[0]) {
    ImGui::SetNextItemWidth(68);
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

} // namespace gui
