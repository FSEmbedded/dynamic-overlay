#pragma once

#include <string>
#include <string_view>

// Extract boot device identity from /proc/cmdline root= parameter.
// Used by PersistentMemDetector to locate the correct storage device.
namespace PersistentMemDetector {
namespace detail {

[[nodiscard]] bool parse_emmc_device(std::string_view cmdline, std::string &out) noexcept;
[[nodiscard]] bool parse_nand_device(std::string_view cmdline, std::string &out) noexcept;

} // namespace detail
} // namespace PersistentMemDetector
