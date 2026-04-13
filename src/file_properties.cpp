#include "file_properties.h"
#include "logging.h"

#include <cerrno>
#include <cstring>
#include <vector>

bool file_properties::properties_set(const OverlayDescription::Persistent &overlay) noexcept
{
    const auto system_lower_dir = get_system_lower_directory(overlay.lower_directory);

    struct stat info_system_dir {};
    if (::stat(system_lower_dir.c_str(), &info_system_dir) == -1) {
        LOG_ERRNO("stat system lower dir", errno);
        return false;
    }

    struct stat info_upper_dir {};
    if (::stat(overlay.upper_directory.c_str(), &info_upper_dir) == -1) {
        LOG_ERRNO("stat upper dir", errno);
        return false;
    }

    return (info_system_dir.st_uid == info_upper_dir.st_uid) &&
           (info_system_dir.st_gid == info_upper_dir.st_gid) &&
           (info_system_dir.st_mode == info_upper_dir.st_mode);
}

// Sync uid, gid, mode, and xattrs from lower to upper so overlayfs
// presents consistent ownership after the first mount.
Error file_properties::copy_properties_lower_to_upper(const OverlayDescription::Persistent &overlay) noexcept
{
    const auto system_lower_dir = get_system_lower_directory(overlay.lower_directory);

    struct stat info_system_dir {};
    if (::stat(system_lower_dir.c_str(), &info_system_dir) == -1) {
        LOG_ERRNO("stat system lower dir", errno);
        return Error::stat_failed;
    }

    if (::chmod(overlay.upper_directory.c_str(), info_system_dir.st_mode) == -1) {
        LOG_ERRNO("chmod upper dir", errno);
        return Error::chmod_failed;
    }

    if (::chown(overlay.upper_directory.c_str(), info_system_dir.st_uid, info_system_dir.st_gid) == -1) {
        LOG_ERRNO("chown upper dir", errno);
        return Error::chown_failed;
    }

    copy_extended_attributes(system_lower_dir, overlay.upper_directory);

    return Error::none;
}

void file_properties::copy_extended_attributes(std::string_view source_dir,
                                                std::string_view target_dir) noexcept
{
    const std::string src(source_dir);
    const auto list_size = ::listxattr(src.c_str(), nullptr, 0);
    if (list_size <= 0) {
        return;
    }

    std::vector<char> list_buffer(static_cast<std::size_t>(list_size));
    if (::listxattr(src.c_str(), list_buffer.data(), list_buffer.size()) == -1) {
        LOG_WARNING("error retrieving xattr list for " + src);
        return;
    }

    for (const char *attr = list_buffer.data(),
                    *end = attr + static_cast<std::size_t>(list_size);
         attr < end;) {
        const auto attr_len = std::strlen(attr);
        if (attr_len > 0) {
            copy_single_attribute(source_dir, target_dir, attr);
        }
        attr += attr_len + 1;
    }
}

void file_properties::copy_single_attribute(std::string_view source_dir,
                                             std::string_view target_dir,
                                             const char *attr_name) noexcept
{
    const std::string src(source_dir);
    const std::string tgt(target_dir);

    const auto value_size = ::getxattr(src.c_str(), attr_name, nullptr, 0);
    if (value_size == -1) {
        LOG_WARNING(std::string("could not read xattr '") + attr_name + "'");
        return;
    }

    std::vector<char> value_buffer(static_cast<std::size_t>(value_size));
    if (::getxattr(src.c_str(), attr_name, value_buffer.data(), value_buffer.size()) == -1) {
        LOG_WARNING(std::string("could not read xattr value for '") + attr_name + "'");
        return;
    }

    if (::setxattr(tgt.c_str(), attr_name, value_buffer.data(),
                    static_cast<std::size_t>(value_size), 0) == -1) {
        LOG_WARNING(std::string("could not set xattr '") + attr_name + "'");
    }
}

std::string file_properties::get_system_lower_directory(std::string_view lowerdir_string) noexcept
{
    if (const auto last_colon_pos = lowerdir_string.rfind(':');
        last_colon_pos != std::string_view::npos) {
        return std::string(lowerdir_string.substr(last_colon_pos + 1));
    }
    return std::string(lowerdir_string);
}
