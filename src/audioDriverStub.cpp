#include <memory>

#include "binaural/audioDriver.hpp"
#include "binaural/wavDriver.hpp"

namespace binaural {

std::unique_ptr<IAudioDriver> createPortAudioDriver()
{
    return std::make_unique<WavFileDriver>();
}

}  // namespace binaural
