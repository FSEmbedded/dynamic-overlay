#pragma once

#include "error.h"
#include "mount.h"

#include <string>
#include <string_view>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/xattr.h>
}

namespace file_properties
{
    [[nodiscard]] bool properties_set(const OverlayDescription::Persistent &overlay) noexcept;

    [[nodiscard]] Error copy_properties_lower_to_upper(const OverlayDescription::Persistent &overlay) noexcept;

    [[nodiscard]] std::string get_system_lower_directory(std::string_view lowerdir_string) noexcept;

    void copy_extended_attributes(std::string_view source_dir, std::string_view target_dir) noexcept;

    void copy_single_attribute(std::string_view source_dir, std::string_view target_dir,
                               const char *attr_name) noexcept;
}
