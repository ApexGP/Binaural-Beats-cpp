#pragma once

#include <vector>

namespace binaural {

/// 正弦查找表，整数索引对应 [0, 2π)
class SinTable {
public:
    explicit SinTable(int size = 1440);

    float sinFastInt(int angle) const;
    float cosFastInt(int angle) const;
    /// phase in [0, 1) -> sin(2*pi*phase), 线性插值保证亚 Hz 精度
    float sinFastFloat(float phase) const;

    int size() const { return static_cast<int>(tableSin_.size()); }

private:
    std::vector<float> tableSin_;
    std::vector<float> tableCos_;
    int size_;
};

}  // namespace binaural
