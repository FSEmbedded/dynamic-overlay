#pragma once

#include "config.h"
#include "error.h"
#include "mount.h"
#include "u-boot.h"
#include "ini_parser.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

// State machine values from fs-updater-lib updateDefinitions.h
inline constexpr uint8_t ROLLBACK_APP_FW_REBOOT_PENDING = 9;
inline constexpr uint8_t INCOMPLETE_APP_FW_ROLLBACK = 12;

class DynamicMounting
{
    std::vector<std::string> overlay_application_;
    std::map<std::string, OverlayDescription::Persistent> overlay_persistent_;
    const std::string overlay_workdir_;
    const std::string overlay_upperdir_;
    const std::string appimage_currentdir_;
    UBoot &uboot_handler_;

    std::vector<OverlayDescription::ReadOnly> additional_lower_directory_to_persistent_;

    [[nodiscard]] Error mount_application() const noexcept;
    [[nodiscard]] Error read_and_parse_ini() noexcept;
    [[nodiscard]] Error parse_default_config() noexcept;
    [[nodiscard]] Error mount_overlay_read_only(bool application_mounted_overlay_parsed) noexcept;
    [[nodiscard]] Error mount_overlay_persistent() noexcept;
    [[nodiscard]] bool detect_failedUpdate_app_fw_reboot() const noexcept;
    [[nodiscard]] Error determine_application_image(std::string &out) const noexcept;

    static bool is_path_mounted(std::string_view path) noexcept;
    static bool has_identical_paths(std::string_view lower_dir) noexcept;

public:
    DynamicMounting(UBoot &uboot) noexcept;
    ~DynamicMounting() noexcept = default;

    DynamicMounting(const DynamicMounting &) = delete;
    DynamicMounting &operator=(const DynamicMounting &) = delete;
    DynamicMounting(DynamicMounting &&) = delete;
    DynamicMounting &operator=(DynamicMounting &&) = delete;

    [[nodiscard]] Error application_image() noexcept;

    [[nodiscard]] Error add_lower_dir_readonly_memory(const OverlayDescription::ReadOnly &container) noexcept;
};
