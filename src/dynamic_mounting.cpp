#include "dynamic_mounting.h"
#include "persistent_mem_detector.h"

// Standard C++ headers
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <numeric>
#include <unordered_map>
#include <regex>
#include <sstream>
#include <set>
#include <memory>
#include <utility>
#include <string>

// Third party headers
#include <inicpp/inicpp.h>

#ifndef APP_IMAGE_DIR
#define APP_IMAGE_DIR "/rw_fs/root/application/"
#endif

#define ROLLBACK_APP_FW_REBOOT_PENDING 9
#define INCOMPLETE_APP_FW_ROLLBACK 12

namespace Config
{
    constexpr int MAX_OVERLAY_COUNT = 8;
}

DynamicMounting::DynamicMounting(const std::shared_ptr<UBoot> &uboot) : overlay_workdir(DEFAULT_WORKDIR_PATH),
                                                                        overlay_upperdir(DEFAULT_UPPERDIR_PATH),
                                                                        appimage_currentdir(DEFAULT_APPLICATION_PATH),
                                                                        application_image_folder(std::regex("ApplicationFolder")),
                                                                        persistent_memory_image(std::regex("PersistentMemory\\..*")),
                                                                        uboot_handler(uboot)
{
    if (!uboot)
    {
        throw DynamicMountingException("Invalid UBoot handler provided");
    }
}

void print_directory_contents(const std::string &path)
{
    try
    {
        // Check if directory exists
        if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
        {
            std::cerr << "Path does not exist or is not a directory: " << path << std::endl;
            return;
        }

        std::cout << "Contents of directory: " << path << std::endl;

        // Iterate through directory entries
        for (const auto &entry : std::filesystem::directory_iterator(path))
        {
            // Get entry type as string
            std::string type = "Unknown";
            if (std::filesystem::is_regular_file(entry))
            {
                type = "File";
            }
            else if (std::filesystem::is_directory(entry))
            {
                type = "Directory";
            }
            else if (std::filesystem::is_symlink(entry))
            {
                type = "Symlink";
            }

            // Print entry information
            std::cout << type << ": " << entry.path().filename().string();

            // If it's a file, print size
            if (std::filesystem::is_regular_file(entry))
            {
                std::cout << " (Size: " << std::filesystem::file_size(entry) << " bytes)";
            }

            std::cout << std::endl;
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

std::string DynamicMounting::determine_application_image() const
{
    const char application = uboot_handler->getVariable("application", std::vector<char>({'A', 'B'}));
    std::string application_image = "app_a.squashfs"; // Default image

    // Check for failed update reboot condition
    const bool is_failed_update = detect_failedUpdate_app_fw_reboot();

    if (!is_failed_update)
    {
        // Normal case - use the image based on current application variable
        if (application == 'B')
        {
            application_image = "app_b.squashfs";
        }
    }
    else
    {
        // Failed update case - check for rollback state
        const std::vector<uint8_t> allowed_update_reboot_state_variables({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
        uint8_t update_reboot_state =
            uboot_handler->getVariable("update_reboot_state", allowed_update_reboot_state_variables);

        // Handle rollback states
        if ((application == 'B') &&
            ((update_reboot_state == ROLLBACK_APP_FW_REBOOT_PENDING) ||
             (update_reboot_state == INCOMPLETE_APP_FW_ROLLBACK)))
        {
            application_image = "app_b.squashfs";
        }
        else if ((application == 'A') &&
                 ((update_reboot_state != ROLLBACK_APP_FW_REBOOT_PENDING) &&
                  (update_reboot_state != INCOMPLETE_APP_FW_ROLLBACK)))
        {
            application_image = "app_b.squashfs";
        }
        // Otherwise default to app_a.squashfs (already set above)
    }

    return application_image;
}

void DynamicMounting::mount_application() const
{
    try
    {
        Mount mount;
        // print_directory_contents("/etc");

        // Determine which application image to use
        std::string application_image = determine_application_image();
        std::filesystem::path app_mount_dir(APP_IMAGE_DIR);
        std::filesystem::path image_path = app_mount_dir / application_image;

        // Ensure the image file exists
        if (!std::filesystem::exists(image_path))
        {
            throw MountException("Application image not found: " + image_path.string());
        }

#ifdef DEBUG
        // Mount the application image
        std::cout << "Mounting application image: " << image_path.string() << std::endl;
#endif
        mount.mount_application_image(image_path);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error mounting application: " << e.what() << std::endl;
        throw MountException(std::string("Failed to mount application: ") + e.what());
    }
}

void DynamicMounting::read_and_parse_ini()
{
    try
    {
        const std::filesystem::path config_path{DEFAULT_OVERLAY_PATH};
        if (!std::filesystem::exists(config_path))
        {
            throw ConfigException("overlay.ini not found at " + std::string(DEFAULT_OVERLAY_PATH));
        }

        inicpp::config overlay_config = inicpp::parser::load_file(DEFAULT_OVERLAY_PATH);

        // Clear existing data to prevent duplication during reloads
        overlay_application.clear();
        overlay_persistent.clear();

        for (const auto &section : overlay_config)
        {
            if (std::regex_match(section.get_name(), application_image_folder))
            {
                // Parse ApplicationFolder section
                parse_application_folder_section(section);
            }
            else if (std::regex_match(section.get_name(), persistent_memory_image))
            {
                // Parse PersistentMemory sections
                parse_persistent_memory_section(section);
            }
            else
            {
                throw ConfigException("Unknown section in overlay.ini: " + section.get_name());
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error parsing overlay.ini: " << e.what() << std::endl;
        throw ConfigException(std::string("Failed to parse overlay.ini: ") + e.what());
    }
}

void DynamicMounting::parse_application_folder_section(const inicpp::section &section)
{
    for (const auto &entry : section)
    {
        if (!entry.is_list())
        {
            overlay_application.emplace_back(entry.get<inicpp::string_ini_t>());
        }
        else
        {
            throw ConfigException("List elements not supported in ApplicationFolder section");
        }
    }
}

void DynamicMounting::parse_persistent_memory_section(const inicpp::section &section)
{
    auto &persistent_section = overlay_persistent[section.get_name()];

    std::unordered_map<std::string, std::string *> field_map = {
        {"lowerdir", &persistent_section.lower_directory},
        {"upperdir", &persistent_section.upper_directory},
        {"workdir", &persistent_section.work_directory},
        {"mergedir", &persistent_section.merge_directory}};

    for (const auto &entry : section)
    {
        const auto &name = entry.get_name();
        if (auto it = field_map.find(name); it != field_map.end())
        {
            *it->second = entry.get<inicpp::string_ini_t>();
        }
        else
        {
            throw ConfigException("Unknown entry in section " + section.get_name() +
                                  ": " + entry.get<inicpp::string_ini_t>());
        }
    }

    // Validate required fields
    std::vector<std::string> required_fields = {"lowerdir", "upperdir", "workdir", "mergedir"};
    for (const auto &field : required_fields)
    {
        auto it = field_map.find(field);
        if (it != field_map.end() && it->second->empty())
        {
            throw ConfigException("Missing required field '" + field + "' in section " + section.get_name());
        }
    }
}

void DynamicMounting::mount_overlay_read_only(bool application_mounted_overlay_parsed)
{
    Mount mount;

    // Function to check if path is already mounted
    auto is_path_mounted = [](const std::string &path) -> bool
    {
        try
        {
            // Read /proc/mounts to check if path is already mounted
            std::ifstream mounts_file("/proc/mounts");
            std::string line;
            while (std::getline(mounts_file, line))
            {
                if (line.find(" " + path + " ") != std::string::npos &&
                    line.find("overlay") != std::string::npos)
                {
                    return true;
                }
            }
            return false;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error checking mount status: " << e.what() << std::endl;
            return false;
        }
    };

    // Check for identical path (to avoid /etc:/etc issue)
    auto has_identical_paths = [](const std::string &lower_dir) -> bool
    {
        std::vector<std::string> paths;
        std::istringstream ss(lower_dir);
        std::string path;

        while (std::getline(ss, path, ':'))
        {
            paths.push_back(path);
        }

        std::sort(paths.begin(), paths.end());
        auto duplicate = std::adjacent_find(paths.begin(), paths.end());
        return duplicate != paths.end();
    };

    // Verify all directories in path exist
    auto verify_paths_exist = [](const std::string &lower_dir) -> bool
    {
        std::istringstream ss(lower_dir);
        std::string path;

        while (std::getline(ss, path, ':'))
        {
            if (!std::filesystem::exists(path))
            {
                std::cerr << "Warning: Path does not exist: " << path << std::endl;
                return false;
            }
        }
        return true;
    };

    // Function to mount ramdisk overlays
    auto mount_ramdisk = [this, &mount, &is_path_mounted, &has_identical_paths, &verify_paths_exist]()
    {
        int successful_mounts = 0;
        std::vector<std::string> failed_mounts;

        for (const auto &add_entry : additional_lower_directory_to_persistent)
        {
            // Check if this entry is already used
            auto it = std::find_if(used_entries_application_overlay.begin(),
                                   used_entries_application_overlay.end(),
                                   [&add_entry](const auto &entry)
                                   {
                                       return *entry == add_entry;
                                   });

            if (it == used_entries_application_overlay.end())
            {
                try
                {
                    // Skip if mount point is already mounted
                    if (is_path_mounted(add_entry.merge_directory))
                    {
#if 1 // def DEBUG
                        std::cout << "mount_overlay_read_only " << std::endl;
                        std::cout << "Skipping already mounted directory: " << add_entry.merge_directory << std::endl;
#endif
                        continue;
                    }

                    // Create modified entry with proper paths
                    OverlayDescription::ReadOnly modified_entry;
                    modified_entry.merge_directory = add_entry.merge_directory;

                    // Build lower directory path properly - remove duplicates
                    std::vector<std::string> unique_paths;
                    if (std::filesystem::exists(add_entry.merge_directory))
                    {
                        unique_paths.push_back(add_entry.merge_directory);
                    }
                    if (std::filesystem::exists(add_entry.lower_directory))
                    {
                        unique_paths.push_back(add_entry.lower_directory);
                    }

                    if (unique_paths.empty())
                    {
                        std::cerr << "Warning: No valid paths for overlay, skipping: "
                                  << add_entry.merge_directory << std::endl;
                        failed_mounts.push_back(add_entry.merge_directory);
                        continue;
                    }

                    // Remove duplicates while preserving order
                    auto last = std::unique(unique_paths.begin(), unique_paths.end());
                    unique_paths.erase(last, unique_paths.end());

                    // Join paths with ':'
                    modified_entry.lower_directory = std::accumulate(
                        std::next(unique_paths.begin()),
                        unique_paths.end(),
                        *unique_paths.begin(),
                        [](const std::string &a, const std::string &b)
                        {
                            return a + ":" + b;
                        });

                    // Skip if lower directories are identical (avoid /etc:/etc)
                    if (has_identical_paths(modified_entry.lower_directory))
                    {
                        std::cout << "Skipping overlay with identical paths: "
                                  << modified_entry.lower_directory << std::endl;
                        continue;
                    }

                    // Verify all paths exist
                    if (!verify_paths_exist(modified_entry.lower_directory))
                    {
                        std::cerr << "Warning: Some lower directories don't exist for "
                                  << modified_entry.merge_directory << ", skipping." << std::endl;
                        failed_mounts.push_back(modified_entry.merge_directory);
                        continue;
                    }

#ifdef DEBUG
                    std::cout << "Setting up ramdisk overlay mount:" << std::endl
                              << "- merge point: " << modified_entry.merge_directory << std::endl
                              << "- lower dirs: " << modified_entry.lower_directory << std::endl;
#endif
                    // Create merge directory if it doesn't exist
                    try
                    {
                        if (!std::filesystem::exists(modified_entry.merge_directory))
                        {
                            std::filesystem::create_directories(modified_entry.merge_directory);
                        }

                        mount.mount_overlay_readonly(modified_entry);
                        successful_mounts++;
                    }
                    catch (const std::filesystem::filesystem_error &fs_err)
                    {
                        std::cerr << "Failed to create directory " << modified_entry.merge_directory
                                  << ": " << fs_err.what() << std::endl;
                        failed_mounts.push_back(modified_entry.merge_directory);
                    }
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Warning: Failed to mount ramdisk overlay for "
                              << add_entry.merge_directory << ": " << e.what() << std::endl;
                    failed_mounts.push_back(add_entry.merge_directory);
                    // Continue with other mounts instead of failing completely
                }
            }
        }

        additional_lower_directory_to_persistent.clear();
        used_entries_application_overlay.clear();

        if (!failed_mounts.empty())
        {
            std::cerr << "Warning: Failed to mount " << failed_mounts.size()
                      << " ramdisk overlays of " << (successful_mounts + failed_mounts.size())
                      << " total." << std::endl;
        }
    };

    // Count successful and failed mounts for reporting
    int successful_app_mounts = 0;
    std::vector<std::string> failed_app_mounts;

    // Mount application overlays if requested and overlay.ini was parsed
    if (application_mounted_overlay_parsed)
    {
        try
        {
            for (const auto &entry : overlay_application)
            {
                // Check if we've already mounted too many overlays
                if (successful_app_mounts >= Config::MAX_OVERLAY_COUNT)
                {
                    std::cout << "Warning: Maximum overlay count reached. Skipping remaining overlay mounts." << std::endl;
                    break;
                }

                // Skip if mount point is already mounted
                if (is_path_mounted(entry))
                {
#ifdef DEBUG
                    std::cout << "Skipping already mounted directory: " << entry << std::endl;
#endif
                    continue;
                }

                OverlayDescription::ReadOnly overlay_desc;
                overlay_desc.merge_directory = entry;

                // Build list of potential lower directories
                std::vector<std::string> potential_paths;

                // Application path takes precedence
                std::string app_path = std::string(DEFAULT_APPLICATION_PATH) + entry;
                if (std::filesystem::exists(app_path))
                {
                    potential_paths.push_back(app_path);
                }

                // Original path comes second if different from app_path
                if (std::filesystem::exists(entry) && entry != app_path)
                {
                    potential_paths.push_back(entry);
                }

                if (potential_paths.empty())
                {
                    std::cerr << "Warning: No valid source paths for " << entry << ", skipping." << std::endl;
                    failed_app_mounts.push_back(entry);
                    continue;
                }

                // Remove duplicates while preserving order
                auto last = std::unique(potential_paths.begin(), potential_paths.end());
                potential_paths.erase(last, potential_paths.end());

                // Create lower_directory string with correct order
                overlay_desc.lower_directory = std::accumulate(
                    std::next(potential_paths.begin()),
                    potential_paths.end(),
                    *potential_paths.begin(),
                    [](const std::string &a, const std::string &b)
                    {
                        return a + ":" + b;
                    });

                // Skip if lower directories are identical
                if (has_identical_paths(overlay_desc.lower_directory))
                {
                    std::cout << "Skipping overlay with identical paths: "
                              << overlay_desc.lower_directory << std::endl;
                    continue;
                }

                // Verify all paths exist
                if (!verify_paths_exist(overlay_desc.lower_directory))
                {
                    std::cerr << "Warning: Some lower directories don't exist for "
                              << overlay_desc.merge_directory << ", skipping." << std::endl;
                    failed_app_mounts.push_back(entry);
                    continue;
                }

#ifdef DEBUG
                std::cout << "Setting up overlay mount:" << std::endl
                          << "- merge point: " << overlay_desc.merge_directory << std::endl
                          << "- lower dirs: " << overlay_desc.lower_directory << std::endl;
#endif

                // Ensure merge directory exists
                try
                {
                    if (!std::filesystem::exists(overlay_desc.merge_directory))
                    {
                        std::cout << "Creating merge directory: " << overlay_desc.merge_directory << std::endl;
                        std::filesystem::create_directories(overlay_desc.merge_directory);
                    }

                    // Try to mount the overlay
                    try
                    {
                        mount.mount_overlay_readonly(overlay_desc);
                        successful_app_mounts++;
                    }
                    catch (const BadOverlayMountReadOnly &mount_e)
                    {

                        if (mount_e.get_errno() == EBUSY)
                        {
                            // Already mounted, skip this entry
                            continue;
                        }

                        std::cerr << "Error mounting " << overlay_desc.merge_directory
                                  << ": " << mount_e.what() << std::endl;
                        failed_app_mounts.push_back(entry);
                    }
                    catch (const std::exception &mount_e)
                    {
                        std::string error_msg = mount_e.what();
                        if (error_msg.find("maximum fs stacking depth exceeded") != std::string::npos)
                        {
                            std::cerr << "Warning: Maximum filesystem stacking depth exceeded for "
                                      << overlay_desc.merge_directory << ". Skipping." << std::endl;
                        }
                        else
                        {
                            std::cerr << "Error mounting " << overlay_desc.merge_directory
                                      << ": " << error_msg << std::endl;
                            failed_app_mounts.push_back(entry);
                        }
                    }
                }
                catch (const std::filesystem::filesystem_error &fs_err)
                {
                    std::cerr << "Failed to create directory " << overlay_desc.merge_directory
                              << ": " << fs_err.what() << std::endl;
                    failed_app_mounts.push_back(entry);
                }
            }

            if (!failed_app_mounts.empty())
            {
                std::cerr << "Warning: Failed to mount " << failed_app_mounts.size()
                          << " application overlays of " << (successful_app_mounts + failed_app_mounts.size())
                          << " total." << std::endl;

                // Continue with ramdisk mounts even if some application overlays failed
                if (successful_app_mounts == 0)
                {
                    std::cerr << "Error: All application overlay mounts failed." << std::endl;
                    mount_ramdisk();
                    throw MountException(std::string("Failed to mount application overlay: All mounts failed"));
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error mounting application overlays: " << e.what() << std::endl;
            // Continue with ramdisk mounts even if application overlay fails
            mount_ramdisk();
            throw MountException(std::string("Failed to mount application overlay: ") + e.what());
        }
    }

    // Always mount ramdisk overlays
    mount_ramdisk();
}

void DynamicMounting::mount_overlay_persistent()
{
    Mount mount;

    // Function to check if path is already mounted
    auto is_path_mounted = [](const std::string &path) -> bool
    {
        try
        {
            // Read /proc/mounts to check if path is already mounted
            std::ifstream mounts_file("/proc/mounts");
            std::string line;
            while (std::getline(mounts_file, line))
            {
                if (line.find(" " + path + " ") != std::string::npos &&
                    line.find("overlay") != std::string::npos)
                {
                    return true;
                }
            }
            return false;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error checking mount status: " << e.what() << std::endl;
            return false;
        }
    };

    // Check for identical path (to avoid /etc:/etc issue)
    auto has_identical_paths = [](const std::string &lower_dir) -> bool
    {
        std::vector<std::string> paths;
        std::istringstream ss(lower_dir);
        std::string path;

        while (std::getline(ss, path, ':'))
        {
            paths.push_back(path);
        }

        std::sort(paths.begin(), paths.end());
        auto duplicate = std::adjacent_find(paths.begin(), paths.end());
        return duplicate != paths.end();
    };

    // Count how many mounts we've done to avoid exceeding fs depth
    int mount_count = 0;

    for (auto &[section_name, section_data] : this->overlay_persistent)
    {
        try
        {
            // Skip if we've reached maximum mounts
            if (mount_count >= Config::MAX_OVERLAY_COUNT)
            {
                std::cout << "Warning: Maximum persistent overlay count reached. Skipping remaining mounts." << std::endl;
                break;
            }

            // Skip if already mounted
            if (is_path_mounted(section_data.merge_directory))
            {
#ifdef DEBUG
                std::cout << "Skipping already mounted persistent overlay: "
                          << section_data.merge_directory << std::endl;
#endif
                // check if was mounted by application folder
                if (std::find(overlay_application.begin(), overlay_application.end(), section_data.merge_directory) != overlay_application.end())
                {
                    // correct lower directory
                    // Application path takes precedence
                    std::string app_path = std::string(DEFAULT_APPLICATION_PATH) + section_data.merge_directory;
                    if (std::filesystem::exists(app_path))
                    {
                        mount.wrapper_c_umount(section_data.merge_directory);
                        section_data.lower_directory = app_path + ":" + section_data.lower_directory;
                    }
                    else
                    {
                        std::cerr << "Warning: No valid source paths for " << section_data.merge_directory << ", skipping." << std::endl;
                        continue;
                    }
                }
                else
                {
                    std::cout << "Skipping already mounted persistent overlay: "
                              << section_data.merge_directory << std::endl;
                    continue;
                }
            }

            // Validate section data before mounting
            if (section_data.merge_directory.empty() ||
                section_data.upper_directory.empty() ||
                section_data.work_directory.empty() ||
                section_data.lower_directory.empty())
            {
                throw MountException("Incomplete persistent overlay configuration for " + section_name);
            }

            // Check for identical lower directories
            if (has_identical_paths(section_data.lower_directory))
            {
                std::cout << "Warning: Skipping persistent overlay with identical paths: "
                          << section_data.lower_directory << std::endl;
                continue;
            }

            // Create directories if they don't exist owner root:root
            try
            {
                if (!std::filesystem::exists(section_data.merge_directory))
                {
                    std::cout << "Creating merge directory: " << section_data.merge_directory << std::endl;
                    std::filesystem::create_directories(section_data.merge_directory);
                }
            }
            catch (const std::filesystem::filesystem_error &fs_err)
            {
                std::cerr << "Failed to create directory " << section_data.merge_directory
                          << ": " << fs_err.what() << std::endl;
                continue;
            }

            std::filesystem::create_directories(section_data.merge_directory);
            std::filesystem::create_directories(section_data.upper_directory);
            std::filesystem::create_directories(section_data.work_directory);

            // Ensure upper and work directories are on the same filesystem
            std::error_code ec;
            if (std::filesystem::space(section_data.upper_directory, ec).available == 0 ||
                std::filesystem::space(section_data.work_directory, ec).available == 0)
            {
                std::cerr << "Warning: Upper or work directory has no available space. "
                          << "This may cause mount to fail." << std::endl;
            }
#ifdef DEBUG
            // Mount the persistent overlay
            std::cout << "Mounting persistent overlay for " << section_name << std::endl
                      << "- merge point: " << section_data.merge_directory << std::endl
                      << "- lower dirs: " << section_data.lower_directory << std::endl
                      << "- upper dir: " << section_data.upper_directory << std::endl
                      << "- work dir: " << section_data.work_directory << std::endl;
#endif

            mount.mount_overlay_persistent(section_data);
            mount_count++;
        }
        catch (const std::exception &e)
        {
            std::string error_msg = e.what();
            // Special handling for "maximum fs stacking depth exceeded"
            if (error_msg.find("maximum fs stacking depth exceeded") != std::string::npos)
            {
                std::cerr << "Warning: Maximum filesystem stacking depth exceeded for "
                          << section_name << ". Skipping remaining mounts." << std::endl;
                break;
            }
            else
            {
                std::cerr << "Error mounting persistent overlay " << section_name
                          << ": " << error_msg << std::endl;
                // Continue with other mounts instead of failing completely
            }
        }
    }
}

bool DynamicMounting::detect_failedUpdate_app_fw_reboot() const
{
    // Helper function to split a string by a delimiter
    auto split = [](const std::string &input, char delimiter) -> std::vector<std::string>
    {
        std::vector<std::string> result;
        std::istringstream stream(input);
        std::string token;

        while (std::getline(stream, token, delimiter))
        {
            result.push_back(token);
        }

        return result;
    };

    // Get required variables from UBoot
    const std::string boot_order = uboot_handler->getVariable("BOOT_ORDER", std::vector<std::string>({"A B", "B A"}));
    const std::string boot_order_old = uboot_handler->getVariable("BOOT_ORDER_OLD", std::vector<std::string>({"A B", "B A"}));
    const std::string rauc_cmd = uboot_handler->getVariable("rauc_cmd", std::vector<std::string>({"rauc.slot=A", "rauc.slot=B"}));
    const uint8_t number_of_tries_a = uboot_handler->getVariable("BOOT_A_LEFT", std::vector<uint8_t>({0, 1, 2, 3}));
    const uint8_t number_of_tries_b = uboot_handler->getVariable("BOOT_B_LEFT", std::vector<uint8_t>({0, 1, 2, 3}));

    // Extract current slot from rauc command
    const auto rauc_parts = split(rauc_cmd, '=');
    const std::string current_slot = rauc_parts.size() > 1 ? rauc_parts.back() : "";

    // Extract first slot from old boot order
    const auto boot_order_old_parts = split(boot_order_old, ' ');
    const std::string first_slot_old = boot_order_old_parts.empty() ? "" : boot_order_old_parts.front();

    // Firmware update is considered failed if:
    // 1. Current slot matches the first slot in the old boot order
    // 2. Number of tries for either A or B is 0
    // 3. Boot order has changed
    const bool firmware_update_reboot_failed =
        (current_slot == first_slot_old) &&
        ((number_of_tries_a == 0) || (number_of_tries_b == 0)) &&
        (boot_order_old != boot_order);

    return firmware_update_reboot_failed;
}

void DynamicMounting::application_image()
{
    // Function to clean up temporary application files
    const auto cleanup_tmp_app = [](const std::filesystem::path &path)
    {
        try
        {
            if (std::filesystem::exists(path))
            {
                std::cout << "Removing temporary application file: " << path << std::endl;
                std::filesystem::remove(path);
            }
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            std::cerr << "Warning: Failed to remove tmp.app: " << e.what() << std::endl;
        }
    };

    try
    {
        // Cleanup tmp.app if exists before starting (might be from a previous failed attempt)
        cleanup_tmp_app(std::filesystem::path(APP_IMAGE_DIR) / "tmp.app");

        try
        {
            mount_application();
        }
        catch (const std::exception &e)
        {
            std::cerr << "Warning: Application mount failed: " << e.what() << std::endl;
        }
        try
        {
            read_and_parse_ini();
        }
        catch (const std::exception &e)
        {
            std::cerr << "Warning: Failed to parse overlay.ini: " << e.what() << std::endl;

            // Set up minimal default configuration
            overlay_application.clear();
            overlay_persistent.clear();

            // Add critical system directories that must be mounted
            overlay_application.push_back("/etc");
            overlay_application.push_back("/usr/bin");
        }
        bool overlay_success = false;
        try
        {
            mount_overlay_read_only(true);
            overlay_success = true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Warning: Primary overlay mount failed: " << e.what() << std::endl;

            // If we got "maximum fs stacking depth exceeded", try with fewer mounts
            std::string error_msg = e.what();
            if (error_msg.find("maximum fs stacking depth exceeded") != std::string::npos)
            {

                // Reset overlay configuration to minimal set
                overlay_application.clear();
                overlay_application.push_back("/etc");

                try
                {
                    mount_overlay_read_only(true);
                    overlay_success = true;
                }
                catch (const std::exception &retry_e)
                {
                    std::cerr << "Warning: Minimal overlay mount also failed: " << retry_e.what() << std::endl;
                }
            }
        }

        // overlay_success = false;
        //  Only proceed with persistent mounts if read-only overlays succeeded
        if (overlay_success)
        {
            try
            {
                mount_overlay_persistent();
            }
            catch (const std::exception &e)
            {
                std::cerr << "Warning: Persistent overlay mount failed: " << e.what() << std::endl;
            }
        }
        else
        {
            std::cout << "dynamicoverlay: Skipping persistent overlay mounts due to previous errors" << std::endl;
        }

        // Final cleanup
        cleanup_tmp_app(std::filesystem::path(APP_IMAGE_DIR) / "tmp.app");
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in application_image: " << e.what() << std::endl;

        // Always try to mount read-only overlays as a fallback
        try
        {
            mount_overlay_read_only(false);
        }
        catch (const std::exception &fallback_e)
        {
            std::cerr << "Fallback error: " << fallback_e.what() << std::endl;
        }

        // Re-throw the original exception with more context
        throw DynamicMountingException(std::string("Application image setup failed: ") + e.what());
    }
}

void DynamicMounting::add_lower_dir_readonly_memory(const OverlayDescription::ReadOnly &container)
{
    if (container.merge_directory.empty() || container.lower_directory.empty())
    {
        throw ConfigException("Invalid overlay description: merge_directory or lower_directory is empty");
    }

    // Check for duplicates before adding
    auto it = std::find_if(
        additional_lower_directory_to_persistent.begin(),
        additional_lower_directory_to_persistent.end(),
        [&container](const auto &entry)
        {
            return entry.merge_directory == container.merge_directory;
        });

    if (it == additional_lower_directory_to_persistent.end())
    {
        additional_lower_directory_to_persistent.push_back(container);
    }
    else
    {
        std::cerr << "Warning: Duplicate merge directory in overlay: "
                  << container.merge_directory << std::endl;
    }
}

DynamicMounting::~DynamicMounting()
{
}
