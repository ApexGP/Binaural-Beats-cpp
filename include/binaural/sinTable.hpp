#pragma once

#include <vector>

namespace binaural {

/// 正弦查找表，整数索引对应 [0, 2π)
class SinTable {
public:
    explicit SinTable(int size = 1440);

    float sinFastInt(int angle) const;
    float cosFastInt(int angle) const;

    int size() const { return static_cast<int>(tableSin_.size()); }

private:
    std::vector<float> tableSin_;
    std::vector<float> tableCos_;
    int size_;
};

}  // namespace binaural
