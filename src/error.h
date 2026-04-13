#pragma once

#include <cstdint>
#include <string_view>

// ABI-stable error codes — values are explicit and append-only.
// Used as preinit exit codes; changing values breaks boot diagnostics.
enum class Error : uint8_t {
    none                   = 0,
    mount_failed           = 1,
    umount_failed          = 2,
    loop_device_failed     = 3,
    overlay_mount_failed   = 4,
    directory_create_failed = 5,
    config_invalid         = 6,
    config_not_found       = 7,
    config_parse_failed    = 8,
    uboot_init_failed      = 9,
    uboot_read_failed      = 10,
    uboot_var_not_found    = 11,
    uboot_var_invalid      = 12,
    permission_denied      = 13,
    stat_failed            = 14,
    chmod_failed           = 15,
    chown_failed           = 16,
    open_failed            = 17,
    read_failed            = 18,
    write_failed           = 19,
    close_failed           = 20,
    path_not_found         = 21,
    memory_detect_failed   = 22,
    blkid_failed           = 23,
    symlink_failed         = 24,
    copy_failed            = 25,
    rename_failed          = 26,
    already_mounted        = 27,
    not_mounted            = 28,
    max_overlay_depth      = 29,
    invalid_argument       = 30,
    cert_store_failed      = 31,
    json_parse_failed      = 32,
    remove_failed          = 33,
};

[[nodiscard]] inline constexpr std::string_view error_to_string(Error e) noexcept
{
    switch (e) {
    case Error::none:                  return "no error";
    case Error::mount_failed:          return "mount failed";
    case Error::umount_failed:         return "umount failed";
    case Error::loop_device_failed:    return "loop device operation failed";
    case Error::overlay_mount_failed:  return "overlay mount failed";
    case Error::directory_create_failed: return "directory creation failed";
    case Error::config_invalid:        return "configuration invalid";
    case Error::config_not_found:      return "configuration file not found";
    case Error::config_parse_failed:   return "configuration parse failed";
    case Error::uboot_init_failed:     return "U-Boot init failed";
    case Error::uboot_read_failed:     return "U-Boot read failed";
    case Error::uboot_var_not_found:   return "U-Boot variable not found";
    case Error::uboot_var_invalid:     return "U-Boot variable invalid";
    case Error::permission_denied:     return "permission denied";
    case Error::stat_failed:           return "stat failed";
    case Error::chmod_failed:          return "chmod failed";
    case Error::chown_failed:          return "chown failed";
    case Error::open_failed:           return "open failed";
    case Error::read_failed:           return "read failed";
    case Error::write_failed:          return "write failed";
    case Error::close_failed:          return "close failed";
    case Error::path_not_found:        return "path not found";
    case Error::memory_detect_failed:  return "persistent memory detection failed";
    case Error::blkid_failed:          return "blkid lookup failed";
    case Error::symlink_failed:        return "symlink creation failed";
    case Error::copy_failed:           return "file copy failed";
    case Error::rename_failed:         return "file rename failed";
    case Error::already_mounted:       return "already mounted";
    case Error::not_mounted:           return "not mounted";
    case Error::max_overlay_depth:     return "maximum overlay stacking depth exceeded";
    case Error::invalid_argument:      return "invalid argument";
    case Error::cert_store_failed:     return "certificate store operation failed";
    case Error::json_parse_failed:     return "JSON parse failed";
    case Error::remove_failed:         return "file removal failed";
    }
    return "unknown error";
}
