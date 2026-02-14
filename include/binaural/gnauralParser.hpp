#pragma once

#include "period.hpp"
#include <optional>
#include <string>

namespace binaural {

/// 解析 Gnaural 格式（.txt 旧格式 或 .gnaural XML）
std::optional<Program> parseGnaural(const std::string& path);
std::optional<Program> parseGnauralFromString(const std::string& content,
                                              const std::string& pathHint = "");

}  // namespace binaural
