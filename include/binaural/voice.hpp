#pragma once

namespace binaural {

struct BinauralBeatVoice {
    float freqStart = 0.f;
    float freqEnd = 0.f;
    float volume = 0.7f;
    float pitch = -1.f;  // -1 = 使用 voicetoPitch 默认
    bool isochronic = false;
};

}  // namespace binaural
