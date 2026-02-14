#include "binaural/sinTable.hpp"
#include <cmath>

namespace binaural {

namespace {
constexpr double PI = 3.14159265358979323846;
}

SinTable::SinTable(int size) : size_(size) {
    tableSin_.resize(size);
    tableCos_.resize(size);
    const double step = 2.0 * PI / size;
    for (int i = 0; i < size; ++i) {
        tableSin_[i] = static_cast<float>(std::sin(step * i));
        tableCos_[i] = static_cast<float>(std::cos(step * i));
    }
}

float SinTable::sinFastInt(int angle) const {
    int idx = angle % size_;
    if (idx < 0) idx += size_;
    return tableSin_[idx];
}

float SinTable::cosFastInt(int angle) const {
    int idx = angle % size_;
    if (idx < 0) idx += size_;
    return tableCos_[idx];
}

float SinTable::sinFastFloat(float phase) const {
    phase -= std::floor(phase);
    const float scaled = phase * static_cast<float>(size_);
    const int i0 = static_cast<int>(scaled) % size_;
    const int i1 = (i0 + 1) % size_;
    const float frac = scaled - std::floor(scaled);
    return tableSin_[i0] * (1.f - frac) + tableSin_[i1] * frac;
}

}  // namespace binaural
