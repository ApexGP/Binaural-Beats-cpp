#include "gui/guiUtils.hpp"

namespace gui {

const char *getBeatDescription(float hz)
{
    if (hz <= 0.5f)  return "Sub-delta — deep dreamless sleep, bodily restoration";
    if (hz <= 4.0f)  return "Delta 0.5–4 Hz — deep sleep, healing, pain relief, cortisol reduction";
    if (hz <= 8.0f)  return "Theta 4–8 Hz — meditation, creativity, REM sleep, emotional processing";
    if (hz <= 13.0f) return "Alpha 8–13 Hz — relaxed alertness, stress reduction, learning readiness";
    if (hz <= 30.0f) return "Beta 13–30 Hz — focus, concentration, problem solving, active cognition";
    return "Gamma 30+ Hz — high-level integration, peak focus, memory consolidation";
}

const char *getBalanceLabel(float b)
{
    if (b < -0.33f) return "Left";
    if (b > 0.33f)  return "Right";
    return "Center";
}

}  // namespace gui
