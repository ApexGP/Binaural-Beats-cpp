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

}  // namespace binaural
