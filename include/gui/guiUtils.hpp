#pragma once

namespace gui {

// Layout constants
constexpr int WIDTH = 960;
constexpr int HEIGHT = 720;
constexpr float REF_WIDTH = 960.f;
constexpr float REF_HEIGHT = 720.f;
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

}  // namespace gui
