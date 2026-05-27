#include "binaural/sinTable.hpp"

#include <cmath>

namespace binaural {

namespace {
constexpr double PI = 3.14159265358979323846;
}

SinTable::SinTable(int size) : size_(size)
{
    // 多分配一个槽位，存放 sin(0)/cos(0)，消除 sinFastFloat 中 i1 的条件分支
    tableSin_.resize(size + 1);
    tableCos_.resize(size + 1);
    const double step = 2.0 * PI / size;
    for (int i = 0; i < size; ++i) {
        tableSin_[i] = static_cast<float>(std::sin(step * i));
        tableCos_[i] = static_cast<float>(std::cos(step * i));
    }
    tableSin_[size] = tableSin_[0];
    tableCos_[size] = tableCos_[0];
}

float SinTable::sinFastInt(int angle) const
{
    int idx = angle % size_;
    if (idx < 0) idx += size_;
    return tableSin_[idx];
}

float SinTable::cosFastInt(int angle) const
{
    int idx = angle % size_;
    if (idx < 0) idx += size_;
    return tableCos_[idx];
}

float SinTable::sinFastFloat(float phase) const
{
    phase -= std::floor(phase);                           // 归一化到 [0,1)
    const float scaled = phase * static_cast<float>(size_);  // [0, size_)
    const int i0 = static_cast<int>(scaled);                 // floor，scaled < size_ 恒成立
    const int i1 = i0 + 1;                                   // 表多分配一项 tableSin_[size_]=tableSin_[0]，无需分支
    const float frac = scaled - static_cast<float>(i0);      // 无第二次 std::floor
    return tableSin_[i0] * (1.f - frac) + tableSin_[i1] * frac;
}

}  // namespace binaural
