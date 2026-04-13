#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace string_utils {

[[nodiscard]] inline std::string_view trim(std::string_view sv) noexcept
{
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' ||
                           sv.front() == '\r' || sv.front() == '\n')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' ||
                           sv.back() == '\r' || sv.back() == '\n')) {
        sv.remove_suffix(1);
    }
    return sv;
}

[[nodiscard]] inline std::string to_lower(std::string_view sv)
{
    std::string result(sv);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

[[nodiscard]] inline std::vector<std::string> split(std::string_view input, char delimiter) noexcept
{
    std::vector<std::string> result;
    while (!input.empty()) {
        const auto pos = input.find(delimiter);
        if (pos != std::string_view::npos) {
            result.emplace_back(input.substr(0, pos));
            input.remove_prefix(pos + 1);
        } else {
            result.emplace_back(input);
            break;
        }
    }
    return result;
}

[[nodiscard]] inline std::string join(const std::vector<std::string> &parts, char delimiter) noexcept
{
    if (parts.empty()) return {};
    std::string result = parts[0];
    for (std::size_t i = 1; i < parts.size(); ++i) {
        result += delimiter;
        result += parts[i];
    }
    return result;
}

} // namespace string_utils
