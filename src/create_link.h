#pragma once

#include "error.h"
#include "persistent_mem_detector.h"

#include <string>
#include <string_view>

// Generate device-specific config files (fw_env.config, system.conf) by
// copying the template for the detected memory type and rewriting device
// paths (/dev/mmcblkN, /dev/mtdN) to match the actual boot device.
namespace create_link
{
    [[nodiscard]] std::string_view get_fw_env_config(PersistentMemDetector::MemType mem_type) noexcept;

    [[nodiscard]] std::string_view get_system_conf(PersistentMemDetector::MemType mem_type) noexcept;

    [[nodiscard]] Error create_link_to_system_conf(PersistentMemDetector::MemType type,
                                                    std::string_view boot_device) noexcept;

    [[nodiscard]] Error create_link_to_fw_env_conf(PersistentMemDetector::MemType type,
                                                    std::string_view boot_device) noexcept;

    [[nodiscard]] bool isBootDeviceConfigured(std::string_view config_path,
                                               std::string_view expected_boot_device) noexcept;
}
