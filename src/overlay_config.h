#pragma once

#include "error.h"
#include "ini_parser.h"
#include "mount.h"

#include <string_view>

namespace overlay_config {

// Parse one PersistentMemory.* section into a Persistent descriptor.
// Unknown keys, invalid nosuid values, or missing required fields return
// Error::config_invalid.
[[nodiscard]] Error parse_persistent_section(std::string_view section_name,
                                              const ini::Section &section,
                                              OverlayDescription::Persistent &out) noexcept;

} // namespace overlay_config
