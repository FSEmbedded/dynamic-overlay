#include "device_parser.h"

#include <cctype>
#include <string>
#include <string_view>

bool PersistentMemDetector::detail::parse_emmc_device(
    std::string_view cmdline, std::string &out) noexcept
{
    // Look for root=/dev/mmcblkN pattern
    constexpr std::string_view prefix = "root=/dev/mmcblk";
    const auto pos = cmdline.find(prefix);
    if (pos == std::string_view::npos) {
        return false;
    }

    const auto start = pos + std::string_view("root=/dev/").size();
    auto end = start + std::string_view("mmcblk").size();
    // Match mmcblkN where N is a single digit 0-2
    if (end >= cmdline.size() || cmdline[end] < '0' || cmdline[end] > '2') {
        return false;
    }
    ++end;

    out = std::string(cmdline.substr(start, end - start));
    return true;
}

bool PersistentMemDetector::detail::parse_nand_device(
    std::string_view cmdline, std::string &out) noexcept
{
    // Look for root=/dev/ubiblockN_N pattern
    constexpr std::string_view prefix = "root=/dev/ubiblock";
    const auto pos = cmdline.find(prefix);
    if (pos == std::string_view::npos) {
        return false;
    }

    const auto start = pos + std::string_view("root=/dev/").size();
    auto end = start;
    // Match ubiblockN_N
    while (end < cmdline.size()) {
        const char c = cmdline[end];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            ++end;
        } else {
            break;
        }
    }

    out = std::string(cmdline.substr(start, end - start));
    return true;
}
