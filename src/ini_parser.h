#pragma once

#include "error.h"

#include <map>
#include <string>
#include <string_view>

namespace ini {

using Section = std::map<std::string, std::string, std::less<>>;
using Config  = std::map<std::string, Section, std::less<>>;

[[nodiscard]] Error parse_file(std::string_view path, Config &out) noexcept;

[[nodiscard]] Error parse_string(std::string_view content, Config &out) noexcept;

} // namespace ini
