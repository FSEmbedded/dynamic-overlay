#include "dynamic_mounting.h"
#include "persistent_mem_detector.h"
#include "posix_utils.h"
#include "string_utils.h"
#include "overlay_config.h"
#include "logging.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

using string_utils::split;
using string_utils::join;

DynamicMounting::DynamicMounting(UBoot &uboot) noexcept
    : overlay_workdir_(config::default_workdir_path),
      overlay_upperdir_(config::default_upperdir_path),
      appimage_currentdir_(config::default_application_path),
      uboot_handler_(uboot)
{
}

Error DynamicMounting::determine_application_image(std::string &out) const noexcept
{
    char application = 'A';
    Error err = uboot_handler_.getVariable("application",
                                            std::vector<char>({'A', 'B'}),
                                            application);
    if (err != Error::none) {
        return err;
    }

    out = "app_a.squashfs";

    const bool is_failed_update = detect_failedUpdate_app_fw_reboot();

    if (!is_failed_update) {
        if (application == 'B') {
            out = "app_b.squashfs";
        }
    } else {
        const std::vector<uint8_t> allowed_states = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        uint8_t update_reboot_state = 0;
        err = uboot_handler_.getVariable("update_reboot_state", allowed_states, update_reboot_state);
        if (err != Error::none) {
            return err;
        }

        if ((application == 'B') &&
            ((update_reboot_state == ROLLBACK_APP_FW_REBOOT_PENDING) ||
             (update_reboot_state == INCOMPLETE_APP_FW_ROLLBACK))) {
            out = "app_b.squashfs";
        } else if ((application == 'A') &&
                   ((update_reboot_state != ROLLBACK_APP_FW_REBOOT_PENDING) &&
                    (update_reboot_state != INCOMPLETE_APP_FW_ROLLBACK))) {
            out = "app_b.squashfs";
        }
    }

    return Error::none;
}

Error DynamicMounting::mount_application() const noexcept
{
    Mount mount;

    std::string application_image;
    Error err = determine_application_image(application_image);
    if (err != Error::none) {
        LOG_ERROR("failed to determine application image");
        return err;
    }

    const std::string image_path = std::string(config::app_image_dir) + application_image;

    if (!posix_utils::path_exists(image_path)) {
        LOG_ERROR("application image not found: " + image_path);
        return Error::path_not_found;
    }

    LOG_DEBUG("mounting application image: " + image_path);

    err = mount.mount_application_image(image_path);
    if (err != Error::none) {
        LOG_ERROR("failed to mount application image: " + image_path);
        return err;
    }

    return Error::none;
}

Error DynamicMounting::read_and_parse_ini() noexcept
{
    if (!posix_utils::path_exists(config::default_overlay_path)) {
        LOG_ERROR("overlay.ini not found at " + std::string(config::default_overlay_path));
        return Error::config_not_found;
    }

    ini::Config ini_config;
    const Error err = ini::parse_file(config::default_overlay_path, ini_config);
    if (err != Error::none) {
        LOG_ERROR("failed to parse overlay.ini");
        return err;
    }

    constexpr std::string_view persistent_prefix = "PersistentMemory.";

    overlay_application_.clear();
    overlay_persistent_.clear();

    for (const auto &[section_name, section_data] : ini_config) {
        if (section_name == "ApplicationFolder") {
            for (const auto &[key, value] : section_data) {
                overlay_application_.emplace_back(value);
            }
        } else if (section_name.size() > persistent_prefix.size() &&
                   section_name.compare(0, persistent_prefix.size(), persistent_prefix) == 0) {
            auto &persistent = overlay_persistent_[section_name];

            const Error parse_err = overlay_config::parse_persistent_section(
                section_name, section_data, persistent);
            if (parse_err != Error::none) {
                return parse_err;
            }
        } else {
            LOG_ERROR("unknown section in overlay.ini: " + section_name);
            return Error::config_invalid;
        }
    }

    return Error::none;
}

Error DynamicMounting::parse_default_config() noexcept
{
    ini::Config ini_config;
    const Error err = ini::parse_string(config::default_overlay_config, ini_config);
    if (err != Error::none) {
        LOG_ERROR("failed to parse compiled-in default config");
        return err;
    }

    overlay_application_.clear();
    overlay_persistent_.clear();

    const auto it = ini_config.find("ApplicationFolder");
    if (it != ini_config.end()) {
        for (const auto &[key, value] : it->second) {
            overlay_application_.emplace_back(value);
        }
    }

    return Error::none;
}

bool DynamicMounting::is_path_mounted(std::string_view path) noexcept
{
    std::string mounts_content;
    if (posix_utils::read_file_to_string("/proc/mounts", mounts_content) != Error::none) {
        return false;
    }

    const std::string target = " " + std::string(path) + " ";
    std::string_view remaining(mounts_content);
    while (!remaining.empty()) {
        const auto nl = remaining.find('\n');
        std::string_view line;
        if (nl != std::string_view::npos) {
            line = remaining.substr(0, nl);
            remaining.remove_prefix(nl + 1);
        } else {
            line = remaining;
            remaining = {};
        }

        if (line.find(target) != std::string_view::npos &&
            line.substr(0, line.find(' ')) == "overlay") {
            return true;
        }
    }
    return false;
}

bool DynamicMounting::has_identical_paths(std::string_view lower_dir) noexcept
{
    auto paths = split(lower_dir, ':');
    std::sort(paths.begin(), paths.end());
    auto dup = std::adjacent_find(paths.begin(), paths.end());
    return dup != paths.end();
}

Error DynamicMounting::mount_overlay_read_only(bool application_mounted_overlay_parsed) noexcept
{
    Mount mount;

    // Mount ramdisk overlays (additional lower dirs not used by application)
    auto mount_ramdisk = [this, &mount]() noexcept -> Error {
        int successful_mounts = 0;
        int failed_mounts = 0;

        for (const auto &add_entry : additional_lower_directory_to_persistent_) {
            if (is_path_mounted(add_entry.merge_directory)) {
                LOG_DEBUG("skipping already mounted ramdisk: " + add_entry.merge_directory);
                continue;
            }

            OverlayDescription::ReadOnly modified_entry;
            modified_entry.merge_directory = add_entry.merge_directory;

            std::vector<std::string> unique_paths;
            if (posix_utils::path_exists(add_entry.merge_directory)) {
                unique_paths.push_back(add_entry.merge_directory);
            }
            if (posix_utils::path_exists(add_entry.lower_directory)) {
                unique_paths.push_back(add_entry.lower_directory);
            }

            if (unique_paths.empty()) {
                LOG_WARNING("no valid paths for ramdisk overlay: " + add_entry.merge_directory);
                ++failed_mounts;
                continue;
            }

            auto last = std::unique(unique_paths.begin(), unique_paths.end());
            unique_paths.erase(last, unique_paths.end());
            modified_entry.lower_directory = join(unique_paths, ':');

            if (has_identical_paths(modified_entry.lower_directory)) {
                LOG_DEBUG("skipping ramdisk overlay with identical paths");
                continue;
            }

            // Verify all paths exist
            bool all_exist = true;
            for (const auto &p : unique_paths) {
                if (!posix_utils::path_exists(p)) {
                    all_exist = false;
                    break;
                }
            }
            if (!all_exist) {
                LOG_WARNING("some lower dirs don't exist for " + modified_entry.merge_directory);
                ++failed_mounts;
                continue;
            }

            if (posix_utils::mkdir_p(modified_entry.merge_directory) != Error::none) {
                LOG_WARNING("failed to create merge dir: " + modified_entry.merge_directory);
                ++failed_mounts;
                continue;
            }

            const Error err = mount.mount_overlay_readonly(modified_entry);
            if (err != Error::none) {
                LOG_WARNING("failed to mount ramdisk overlay for " + add_entry.merge_directory);
                ++failed_mounts;
            } else {
                ++successful_mounts;
            }
        }

        additional_lower_directory_to_persistent_.clear();

        if (failed_mounts > 0) {
            LOG_WARNING("failed to mount " + std::to_string(failed_mounts) + " ramdisk overlays of " +
                        std::to_string(successful_mounts + failed_mounts) + " total");
        }

        return Error::none;
    };

    int successful_app_mounts = 0;
    int failed_app_mounts = 0;

    if (application_mounted_overlay_parsed) {
        for (const auto &entry : overlay_application_) {
            if (successful_app_mounts >= config::max_overlay_count) {
                LOG_WARNING("maximum overlay count reached, skipping remaining");
                break;
            }

            if (is_path_mounted(entry)) {
                LOG_DEBUG("skipping already mounted: " + entry);
                continue;
            }

            OverlayDescription::ReadOnly overlay_desc;
            overlay_desc.merge_directory = entry;

            std::vector<std::string> potential_paths;
            const std::string app_path = std::string(config::default_application_path) + entry;

            if (posix_utils::path_exists(app_path)) {
                potential_paths.push_back(app_path);
            }
            if (posix_utils::path_exists(entry) && entry != app_path) {
                potential_paths.push_back(entry);
            }

            if (potential_paths.empty()) {
                LOG_WARNING("no valid source paths for " + entry);
                ++failed_app_mounts;
                continue;
            }

            auto last = std::unique(potential_paths.begin(), potential_paths.end());
            potential_paths.erase(last, potential_paths.end());
            overlay_desc.lower_directory = join(potential_paths, ':');

            if (has_identical_paths(overlay_desc.lower_directory)) {
                LOG_DEBUG("skipping overlay with identical paths: " + overlay_desc.lower_directory);
                continue;
            }

            // Verify paths exist
            bool all_exist = true;
            for (const auto &p : potential_paths) {
                if (!posix_utils::path_exists(p)) {
                    all_exist = false;
                    break;
                }
            }
            if (!all_exist) {
                LOG_WARNING("some lower dirs don't exist for " + overlay_desc.merge_directory);
                ++failed_app_mounts;
                continue;
            }

            LOG_DEBUG("overlay mount: merge=" + overlay_desc.merge_directory +
                      " lower=" + overlay_desc.lower_directory);

            static_cast<void>(posix_utils::mkdir_p(overlay_desc.merge_directory));

            const Error err = mount.mount_overlay_readonly(overlay_desc);
            if (err != Error::none) {
                if (err == Error::already_mounted) {
                    continue;
                }
                LOG_ERROR("error mounting " + overlay_desc.merge_directory);
                ++failed_app_mounts;
            } else {
                ++successful_app_mounts;
            }
        }

        if (failed_app_mounts > 0) {
            LOG_WARNING("failed to mount " + std::to_string(failed_app_mounts) +
                        " application overlays of " +
                        std::to_string(successful_app_mounts + failed_app_mounts) + " total");

            if (successful_app_mounts == 0) {
                LOG_ERROR("all application overlay mounts failed");
                static_cast<void>(mount_ramdisk());
                return Error::overlay_mount_failed;
            }
        }
    }

    return mount_ramdisk();
}

Error DynamicMounting::mount_overlay_persistent() noexcept
{
    Mount mount;
    int mount_count = 0;

    for (auto &[section_name, section_data] : overlay_persistent_) {
        if (mount_count >= config::max_overlay_count) {
            LOG_WARNING("maximum persistent overlay count reached");
            break;
        }

        if (is_path_mounted(section_data.merge_directory)) {
            // Check if was mounted by application folder — needs re-mount with app path
            if (std::find(overlay_application_.begin(), overlay_application_.end(),
                          section_data.merge_directory) != overlay_application_.end()) {
                const std::string app_path = std::string(config::default_application_path) + section_data.merge_directory;
                if (posix_utils::path_exists(app_path)) {
                    const Error umount_err = mount.wrapper_c_umount(section_data.merge_directory);
                    if (umount_err != Error::none) {
                        LOG_WARNING("cannot unmount " + section_data.merge_directory + " for persistent re-mount");
                        continue;
                    }
                    section_data.lower_directory = app_path + ":" + section_data.lower_directory;
                } else {
                    LOG_WARNING("no valid source paths for " + section_data.merge_directory);
                    continue;
                }
            } else {
                LOG_DEBUG("skipping already mounted persistent overlay: " + section_data.merge_directory);
                continue;
            }
        }

        if (section_data.merge_directory.empty() || section_data.upper_directory.empty() ||
            section_data.work_directory.empty() || section_data.lower_directory.empty()) {
            LOG_ERROR("incomplete persistent overlay config for " + section_name);
            continue;
        }

        if (has_identical_paths(section_data.lower_directory)) {
            LOG_WARNING("skipping persistent overlay with identical paths: " + section_data.lower_directory);
            continue;
        }

        static_cast<void>(posix_utils::mkdir_p(section_data.merge_directory));
        static_cast<void>(posix_utils::mkdir_p(section_data.upper_directory));
        static_cast<void>(posix_utils::mkdir_p(section_data.work_directory));

        LOG_DEBUG("mounting persistent overlay for " + section_name);

        const Error err = mount.mount_overlay_persistent(section_data);
        if (err != Error::none) {
            LOG_ERROR("error mounting persistent overlay " + section_name);
            continue;
        }
        ++mount_count;
    }

    return Error::none;
}

bool DynamicMounting::detect_failedUpdate_app_fw_reboot() const noexcept
{
    std::string boot_order;
    if (uboot_handler_.getVariable("BOOT_ORDER",
                                    std::vector<std::string>({"A B", "B A"}),
                                    boot_order) != Error::none) {
        return false;
    }

    std::string boot_order_old;
    if (uboot_handler_.getVariable("BOOT_ORDER_OLD",
                                    std::vector<std::string>({"A B", "B A"}),
                                    boot_order_old) != Error::none) {
        return false;
    }

    std::string rauc_cmd;
    if (uboot_handler_.getVariable("rauc_cmd",
                                    std::vector<std::string>({"rauc.slot=A", "rauc.slot=B"}),
                                    rauc_cmd) != Error::none) {
        return false;
    }

    uint8_t tries_a = 0;
    if (uboot_handler_.getVariable("BOOT_A_LEFT",
                                    std::vector<uint8_t>({0, 1, 2, 3}),
                                    tries_a) != Error::none) {
        return false;
    }

    uint8_t tries_b = 0;
    if (uboot_handler_.getVariable("BOOT_B_LEFT",
                                    std::vector<uint8_t>({0, 1, 2, 3}),
                                    tries_b) != Error::none) {
        return false;
    }

    // Extract slot from rauc_cmd
    const auto rauc_parts = split(rauc_cmd, '=');
    const std::string current_slot = rauc_parts.size() > 1 ? rauc_parts.back() : "";

    // Extract first slot from old boot order
    const auto old_parts = split(boot_order_old, ' ');
    const std::string first_slot_old = old_parts.empty() ? "" : old_parts.front();

    return (current_slot == first_slot_old) &&
           ((tries_a == 0) || (tries_b == 0)) &&
           (boot_order_old != boot_order);
}

// Main entry point. Orchestrates: squashfs mount → overlay parse →
// read-only overlays (app dirs) → persistent overlays (upper+work dirs).
// Falls back to compiled-in defaults if overlay.ini is missing.
Error DynamicMounting::application_image() noexcept
{
    // Cleanup tmp.app before starting
    const std::string tmp_app = std::string(config::app_image_dir) + "tmp.app";
    static_cast<void>(posix_utils::remove_file(tmp_app));

    Error mount_err = mount_application();
    if (mount_err != Error::none) {
        LOG_WARNING("application mount failed");
    }

    Error parse_err = read_and_parse_ini();
    if (parse_err != Error::none) {
        LOG_WARNING("overlay.ini failed, using compiled-in defaults");
        parse_err = parse_default_config();
    }

    bool overlay_success = false;
    Error overlay_err = mount_overlay_read_only(true);
    if (overlay_err == Error::none) {
        overlay_success = true;
    } else {
        LOG_WARNING("primary overlay mount failed");

        // Retry with minimal set
        overlay_application_.clear();
        overlay_application_.push_back("/etc");

        overlay_err = mount_overlay_read_only(true);
        if (overlay_err == Error::none) {
            overlay_success = true;
        } else {
            LOG_WARNING("minimal overlay mount also failed");
        }
    }

    if (overlay_success) {
        const Error persistent_err = mount_overlay_persistent();
        if (persistent_err != Error::none) {
            LOG_WARNING("persistent overlay mount failed");
        }
    } else {
        LOG_INFO("skipping persistent overlay mounts due to previous errors");
    }

    // Final cleanup
    static_cast<void>(posix_utils::remove_file(tmp_app));

    return mount_err;
}

Error DynamicMounting::add_lower_dir_readonly_memory(const OverlayDescription::ReadOnly &container) noexcept
{
    if (container.merge_directory.empty() || container.lower_directory.empty()) {
        LOG_ERROR("invalid overlay description: empty merge_directory or lower_directory");
        return Error::invalid_argument;
    }

    auto it = std::find_if(
        additional_lower_directory_to_persistent_.begin(),
        additional_lower_directory_to_persistent_.end(),
        [&container](const auto &entry) {
            return entry.merge_directory == container.merge_directory;
        });

    if (it == additional_lower_directory_to_persistent_.end()) {
        additional_lower_directory_to_persistent_.push_back(container);
    } else {
        LOG_WARNING("duplicate merge directory in overlay: " + container.merge_directory);
    }

    return Error::none;
}
