#include "gui/guiFonts.hpp"

#include <filesystem>
#include <string>

#include "imgui.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace gui {

namespace {
std::string getFontDir() {
  namespace fs = std::filesystem;
  std::string fontDir;
#ifdef _WIN32
  wchar_t exePath[MAX_PATH];
  if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
    fs::path p(exePath);
    for (int i = 0; i < 3; ++i) {
      auto dir = p.parent_path() / "font";
      if (fs::exists(dir)) return dir.string();
      p = p.parent_path();
    }
  }
#endif
  for (const char *rel : {"font", "../font", "../../font"}) {
    if (fs::exists(rel)) return rel;
  }
  return "";
}
}  // namespace

void loadFontsFromDir(float dpiScale) {
  namespace fs = std::filesystem;
  ImGuiIO &io = ImGui::GetIO();
  std::string fontDir = getFontDir();
  if (fontDir.empty()) {
    io.Fonts->AddFontDefault();
    return;
  }
  if (dpiScale < 0.5f) dpiScale = 1.f;
  const float fontSize = 16.0f * dpiScale;
  static const ImWchar *iconRanges = nullptr;
  static ImVector<ImWchar> iconRangesBuf;
  {
    ImFontGlyphRangesBuilder b;
    b.AddRanges(io.Fonts->GetGlyphRangesDefault());
    b.AddChar(0x23F1);
    b.AddChar(0x22EE);
    b.AddChar(0x25B6);
    b.AddChar(0x221E);  // ∞ infinity
    b.AddChar(0xEB7C);
    b.AddChar(0xEB10);
    b.AddChar(0xEB2C);
    b.BuildRanges(&iconRangesBuf);
    iconRanges = iconRangesBuf.Data;
  }
  const char *defaultOrder[] = {
      "NotoSans-SemiBold.ttf",
      "NotoSansSymbols-SemiBold.ttf",
      "NotoSansSymbols-VariableFont_wght.ttf",
      "JetBrainsMonoNerdFontMono-Regular.ttf",
  };
  ImFont *baseFont = nullptr;
  for (const char *name : defaultOrder) {
    std::string path = fontDir + "/" + name;
    if (fs::exists(path)) {
      baseFont = io.Fonts->AddFontFromFileTTF(path.c_str(), fontSize, nullptr,
                                              iconRanges);
      if (baseFont) break;
    }
  }
  if (!baseFont) {
    for (const auto &e : fs::directory_iterator(fontDir)) {
      if (e.path().extension() == ".ttf" || e.path().extension() == ".otf") {
        baseFont = io.Fonts->AddFontFromFileTTF(e.path().string().c_str(),
                                                fontSize, nullptr, iconRanges);
        if (baseFont) break;
      }
    }
  }
  if (baseFont) {
    ImFontConfig cfg;
    cfg.MergeMode = true;
    const ImWchar *chineseRanges =
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    const char *cjkFonts[] = {
        "NotoSansCJKsc-Regular.otf",
        "NotoSansCJK-Regular.ttc",
        "SourceHanSansSC-Regular.otf",
    };
    bool cjkMerged = false;
    for (const char *name : cjkFonts) {
      std::string path = fontDir + "/" + name;
      if (fs::exists(path)) {
        io.Fonts->AddFontFromFileTTF(path.c_str(), fontSize, &cfg,
                                     chineseRanges);
        cjkMerged = true;
        break;
      }
    }
#ifdef _WIN32
    if (!cjkMerged) {
      const char *winCjkPaths[] = {
          "C:\\Windows\\Fonts\\msyh.ttc",
          "C:\\Windows\\Fonts\\simsun.ttc",
      };
      for (const char *path : winCjkPaths) {
        if (fs::exists(path)) {
          io.Fonts->AddFontFromFileTTF(path, fontSize, &cfg, chineseRanges);
          break;
        }
      }
    }
#endif
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

}  // namespace gui
