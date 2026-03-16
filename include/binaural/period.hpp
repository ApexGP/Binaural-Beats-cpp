#pragma once

#include <string>
#include <vector>

#include "voice.hpp"

namespace binaural {

struct Period {
    enum class Background { None, WhiteNoise, PinkNoise };
    int lengthSec = 0;
    std::vector<BinauralBeatVoice> voices;
    Background background = Background::None;
    float backgroundVol = 0.f;
};

struct Program {
    std::string name;
    std::vector<Period> seq;
};

}  // namespace binaural
