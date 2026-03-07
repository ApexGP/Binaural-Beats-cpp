#pragma once

#include "gui/guiPanels.hpp"
#include <GLFW/glfw3.h>

namespace gui {

struct RenderFrameData {
  AppContext *ctx = nullptr;
  GLFWwindow *window = nullptr;
  binaural::ParameterController *paramController = nullptr;
  binaural::IAudioDriver *driver = nullptr;
};

void doOneRenderFrame(RenderFrameData &data);

} // namespace gui
