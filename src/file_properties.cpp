#include "file_properties.h"
#include <sys/xattr.h>
#include <memory>
#include <vector>
#include <stdexcept>
#include <iostream>

/* The member function properties_set checks if the properties of the lower and upper directories are set correctly.
 * It compares the permissions, owner, and group of both directories.
 * @param overlay Persistent overlay data structure.
 * @return true if the properties are set correctly, false otherwise.
 * @throws ErrnoCstat if stat fails on either directory.
 */
bool file_properties::properties_set(const OverlayDescription::Persistent &overlay)
{
    int ret;
    struct stat info_lower_dir;
    struct stat info_upper_dir;
    /* Check if lower and upper directories exist */
    ret = ::stat(overlay.lower_directory.c_str(), &info_lower_dir);
    if (ret == -1)
    {
        throw ErrnoCstat(errno, overlay.lower_directory);
    }

    ret = ::stat(overlay.upper_directory.c_str(), &info_upper_dir);
    if (ret == -1)
    {
        throw ErrnoCstat(errno, overlay.lower_directory);
    }

    /* Compare the permissions */
    if ((info_lower_dir.st_uid != info_upper_dir.st_uid) ||
        (info_lower_dir.st_gid != info_upper_dir.st_gid))
    {
        return false;
    }

    return true;
}

/* The member function copy_properties_lower_to_upper copies the properties from the lower directory to the upper directory.
 * It copies the permissions, owner, group, and extended attributes (xattr).
 ** @param overlay Persistent overlay data structure.
 * @throws ErrnoCstat if stat fails on the lower directory.
 * @throws ErrnoCchmod if chmod fails on the upper directory.
 */
void file_properties::copy_properties_lower_to_upper(const OverlayDescription::Persistent &overlay)
{
    int ret;
    struct stat info_lower_dir;

    // Copy permissions, owner and group
    ret = ::stat(overlay.lower_directory.c_str(), &info_lower_dir);
    if (ret == -1)
    {
        throw ErrnoCstat(errno, overlay.lower_directory);
    }

    ret = ::chmod(overlay.upper_directory.c_str(), info_lower_dir.st_mode);
    if (ret == -1)
    {
        throw ErrnoCchmod(errno, overlay.upper_directory);
    }

    ret = ::chown(overlay.upper_directory.c_str(), info_lower_dir.st_uid, info_lower_dir.st_gid);
    if (ret == -1)
    {
        throw ErrnoCchown(errno, overlay.upper_directory);
    }

    // Copy extended attributes (xattr)
    ssize_t list_size = ::listxattr(overlay.lower_directory.c_str(), nullptr, 0);
    if (list_size == -1)
    {
        throw std::runtime_error("Error retrieving xattr list size for " + overlay.lower_directory);
    }

    // If no xattrs exist, we can return immediately
    if (list_size == 0)
    {
        return;
    }

    // Get attribute names with consistent error handling
    std::vector<char> list_buffer(list_size);
    list_size = ::listxattr(overlay.lower_directory.c_str(), list_buffer.data(), list_buffer.size());
    if (list_size == -1)
    {
        throw std::runtime_error("Error retrieving xattr list for " + overlay.lower_directory);
    }

    // Process and copy each attribute
    const char *attr = list_buffer.data();
    const char *end = attr + list_size;

    while (attr < end)
    {
        size_t attr_len = strlen(attr);
        if (attr_len > 0)
        {
            // Get attribute value size
            ssize_t value_size = ::getxattr(overlay.lower_directory.c_str(), attr, nullptr, 0);
            if (value_size == -1)
            {
                // Log and continue if an attribute can't be read
                // This is more robust than throwing an error
                std::cerr << "Warning: Could not read xattr '" << attr << "'" << std::endl;
                attr += attr_len + 1;
                continue;
            }

            // Read attribute value
            std::vector<char> value_buffer(value_size);
            value_size = ::getxattr(overlay.lower_directory.c_str(), attr, value_buffer.data(), value_buffer.size());
            if (value_size == -1)
            {
                std::cerr << "Warning: Could not read xattr value for '" << attr << "'" << std::endl;
                attr += attr_len + 1;
                continue;
            }

            // Copy attribute to upper directory
            ret = ::setxattr(overlay.upper_directory.c_str(), attr, value_buffer.data(), value_size, 0);
            if (ret == -1)
            {
                // Log and continue if an attribute can't be set
                std::cerr << "Warning: Could not set xattr '" << attr << "'" << std::endl;
            }
        }

        attr += attr_len + 1;
    }
}
