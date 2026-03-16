#pragma once

#include <GLFW/glfw3.h>

#include "gui/guiPanels.hpp"

namespace gui {

struct RenderFrameData {
    AppContext *ctx = nullptr;
    GLFWwindow *window = nullptr;
    binaural::ParameterController *paramController = nullptr;
    binaural::IAudioDriver *driver = nullptr;
};

void doOneRenderFrame(RenderFrameData &data);

}  // namespace gui
