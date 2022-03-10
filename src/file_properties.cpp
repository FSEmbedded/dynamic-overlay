#include "file_properties.h"

bool file_properties::properties_set(const OverlayDescription::Persistent &overlay)
{
    int ret;
    struct stat info_lower_dir;
    struct stat info_upper_dir;

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

    if ((info_lower_dir.st_uid == info_upper_dir.st_uid) && (info_lower_dir.st_gid == info_upper_dir.st_gid))
    {
        return true;
    }
    else
    {
        return false;
    }

}

void file_properties::copy_properties_lower_to_upper(const OverlayDescription::Persistent &overlay)
{
    int ret;
    struct stat info_lower_dir;

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
}