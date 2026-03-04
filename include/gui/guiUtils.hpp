#pragma once

#include "imgui.h"

namespace gui {

// Layout constants
constexpr int WIDTH = 960;
constexpr int HEIGHT = 720;
constexpr float BEAT_MIN = 0.f;
constexpr float BEAT_MAX = 40.f;
constexpr float BASE_FREQ_MIN = 0.f;
constexpr float BASE_FREQ_MAX = 500.f;
constexpr float BALANCE_MIN = -1.f;
constexpr float BALANCE_MAX = 1.f;
constexpr float VOL_MIN = 0.f;
constexpr float VOL_MAX = 1.2f;

const char *getBeatDescription(float hz);
const char *getBalanceLabel(float b);
void applyDarkTheme();

bool sliderWithButtons(const char *label, float *v, float minV, float maxV,
                       const char *fmt, float step, const char *valueLabel,
                       const char *inputFmt = nullptr,
                       const char *unitSuffix = nullptr);

} // namespace gui
