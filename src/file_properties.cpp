#include "file_properties.h"
#include <sys/xattr.h>
#include <memory>
#include <vector>
#include <stdexcept>
#include <iostream>

bool file_properties::properties_set(const OverlayDescription::Persistent &overlay)
{
    const auto system_lower_dir = get_system_lower_directory(overlay.lower_directory);

    /* Check if system and upper directories exist and get their stats */
    struct stat info_system_dir{};
    if (::stat(system_lower_dir.c_str(), &info_system_dir) == -1)
    {
        throw ErrnoCstat(errno, system_lower_dir);
    }

    struct stat info_upper_dir{};
    if (::stat(overlay.upper_directory.c_str(), &info_upper_dir) == -1)
    {
        throw ErrnoCstat(errno, overlay.upper_directory);
    }

    /* Compare the permissions - upper should match system directory */
    return (info_system_dir.st_uid == info_upper_dir.st_uid) &&
           (info_system_dir.st_gid == info_upper_dir.st_gid) &&
           (info_system_dir.st_mode == info_upper_dir.st_mode);
}

/* The member function copy_properties_lower_to_upper copies the properties from the system directory to the upper directory.
 * This ensures that the upper directory has the correct system permissions (root:root) instead of
 * the potentially incorrect permissions from the user-created application squashfs.
 * @param overlay Persistent overlay data structure.
 * @throws ErrnoCstat if stat fails on the system directory.
 * @throws ErrnoCchmod if chmod fails on the upper directory.
 */
void file_properties::copy_properties_lower_to_upper(const OverlayDescription::Persistent &overlay)
{
    const auto system_lower_dir = get_system_lower_directory(overlay.lower_directory);

    // Copy permissions, owner and group from system directory
    struct stat info_system_dir{};
    if (::stat(system_lower_dir.c_str(), &info_system_dir) == -1)
    {
        throw ErrnoCstat(errno, system_lower_dir);
    }

    if (::chmod(overlay.upper_directory.c_str(), info_system_dir.st_mode) == -1)
    {
        throw ErrnoCchmod(errno, overlay.upper_directory);
    }

    if (::chown(overlay.upper_directory.c_str(), info_system_dir.st_uid, info_system_dir.st_gid) == -1)
    {
        throw ErrnoCchown(errno, overlay.upper_directory);
    }

    // Copy extended attributes (xattr) from system directory
    copy_extended_attributes(system_lower_dir, overlay.upper_directory);
}

void file_properties::copy_extended_attributes(const std::string &source_dir, const std::string &target_dir)
{
    const auto list_size = ::listxattr(source_dir.c_str(), nullptr, 0);
    if (list_size == -1 || list_size == 0)
    {
        // System directories might not have xattrs or error occurred, not critical
        return;
    }

    // Get attribute names
    std::vector<char> list_buffer(static_cast<std::size_t>(list_size));
    if (::listxattr(source_dir.c_str(), list_buffer.data(), list_buffer.size()) == -1)
    {
        std::cerr << "Warning: Error retrieving xattr list for " << source_dir << '\n';
        return;
    }

    // Process and copy each attribute
    for (const char *attr = list_buffer.data(), *end = attr + list_size; attr < end;)
    {
        const auto attr_len = std::strlen(attr);
        if (attr_len > 0)
        {
            copy_single_attribute(source_dir, target_dir, attr);
        }
        attr += attr_len + 1;
    }
}

void file_properties::copy_single_attribute(const std::string &source_dir, const std::string &target_dir, const char *attr_name)
{
    // Get attribute value size
    const auto value_size = ::getxattr(source_dir.c_str(), attr_name, nullptr, 0);
    if (value_size == -1)
    {
        std::cerr << "Warning: Could not read xattr '" << attr_name << "'\n";
        return;
    }

    // Read attribute value
    std::vector<char> value_buffer(static_cast<std::size_t>(value_size));
    if (::getxattr(source_dir.c_str(), attr_name, value_buffer.data(), value_buffer.size()) == -1)
    {
        std::cerr << "Warning: Could not read xattr value for '" << attr_name << "'\n";
        return;
    }

    // Copy attribute to target directory
    if (::setxattr(target_dir.c_str(), attr_name, value_buffer.data(), static_cast<std::size_t>(value_size), 0) == -1)
    {
        std::cerr << "Warning: Could not set xattr '" << attr_name << "'\n";
    }
}

// Helper function to extract the system (last) lower directory from a colon-separated lowerdir string
[[nodiscard]] std::string file_properties::get_system_lower_directory(const std::string &lowerdir_string)
{
    if (const auto last_colon_pos = lowerdir_string.rfind(':'); last_colon_pos != std::string::npos)
    {
        return lowerdir_string.substr(last_colon_pos + 1);
    }
    return lowerdir_string;
}
