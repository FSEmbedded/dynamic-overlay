#include "create_link.h"
#include "config.h"
#include "posix_utils.h"
#include "logging.h"

#include <cstring>
#include <string>
#include <string_view>

extern "C" {
#include <unistd.h>
}

namespace {

// Replace /dev/mmcblkN(pN|bootN) with /dev/<bootDevice>(pN|bootN) in a line
std::string replace_mmc_device(std::string_view line, std::string_view boot_device) noexcept
{
    std::string result;
    result.reserve(line.size());

    std::string_view remaining = line;
    while (!remaining.empty()) {
        // Look for /dev/mmcblk
        const auto pos = remaining.find("/dev/mmcblk");
        if (pos == std::string_view::npos) {
            result.append(remaining);
            break;
        }

        result.append(remaining.substr(0, pos));
        remaining.remove_prefix(pos);

        // Find end of device name: /dev/mmcblkN followed optionally by pN or bootN
        auto end = std::string_view("/dev/mmcblk").size();
        // Skip digits after mmcblk
        while (end < remaining.size() && remaining[end] >= '0' && remaining[end] <= '9') {
            ++end;
        }
        // Check for pN or bootN suffix
        if (end < remaining.size() && (remaining[end] == 'p' ||
            (remaining.size() - end >= 4 && remaining.compare(end, 4, "boot") == 0))) {
            if (remaining[end] == 'p') {
                ++end;
                while (end < remaining.size() && remaining[end] >= '0' && remaining[end] <= '9') {
                    ++end;
                }
            } else {
                end += 4; // "boot"
                while (end < remaining.size() && remaining[end] >= '0' && remaining[end] <= '9') {
                    ++end;
                }
            }
        }

        // Extract the suffix after mmcblkN (pN or bootN)
        const auto dev_prefix_end = std::string_view("/dev/mmcblk").size();
        auto digit_end = dev_prefix_end;
        while (digit_end < end && remaining[digit_end] >= '0' && remaining[digit_end] <= '9') {
            ++digit_end;
        }
        const auto suffix = remaining.substr(digit_end, end - digit_end);

        result.append("/dev/");
        result.append(boot_device);
        result.append(suffix);
        remaining.remove_prefix(end);
    }

    return result;
}

// Replace /dev/mtdN(pN)? with /dev/<mtdDevice>(pN)? in a line
std::string replace_mtd_device(std::string_view line, std::string_view mtd_device) noexcept
{
    std::string result;
    result.reserve(line.size());

    std::string_view remaining = line;
    while (!remaining.empty()) {
        const auto pos = remaining.find("/dev/mtd");
        if (pos == std::string_view::npos) {
            result.append(remaining);
            break;
        }

        result.append(remaining.substr(0, pos));
        remaining.remove_prefix(pos);

        auto end = std::string_view("/dev/mtd").size();
        // Skip digits
        while (end < remaining.size() && remaining[end] >= '0' && remaining[end] <= '9') {
            ++end;
        }
        // Optional pN suffix
        if (end < remaining.size() && remaining[end] == 'p') {
            ++end;
            while (end < remaining.size() && remaining[end] >= '0' && remaining[end] <= '9') {
                ++end;
            }
        }

        result.append("/dev/");
        result.append(mtd_device);
        remaining.remove_prefix(end);
    }

    return result;
}

[[nodiscard]] Error update_mcc_boot_dev_conf(std::string_view config_path,
                                              std::string_view boot_device) noexcept
{
    std::string content;
    Error err = posix_utils::read_file_to_string(config_path, content);
    if (err != Error::none) {
        return err;
    }

    std::string output;
    output.reserve(content.size());

    std::string_view remaining(content);
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

        output.append(replace_mmc_device(line, boot_device));
        output.push_back('\n');
    }

    const std::string tmp_path = std::string(config_path) + ".tmp";
    err = posix_utils::write_string_to_file(tmp_path, output);
    if (err != Error::none) {
        return err;
    }

    err = posix_utils::rename_file(tmp_path, config_path);
    if (err != Error::none) {
        static_cast<void>(posix_utils::remove_file(tmp_path));
        return err;
    }

    ::sync();
    return Error::none;
}

[[nodiscard]] Error update_mtd_boot_dev_conf(std::string_view config_path,
                                              std::string_view mtd_device) noexcept
{
    std::string content;
    Error err = posix_utils::read_file_to_string(config_path, content);
    if (err != Error::none) {
        return err;
    }

    std::string output;
    output.reserve(content.size());

    std::string_view remaining(content);
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

        output.append(replace_mtd_device(line, mtd_device));
        output.push_back('\n');
    }

    const std::string tmp_path = std::string(config_path) + ".tmp";
    err = posix_utils::write_string_to_file(tmp_path, output);
    if (err != Error::none) {
        return err;
    }

    err = posix_utils::rename_file(tmp_path, config_path);
    if (err != Error::none) {
        static_cast<void>(posix_utils::remove_file(tmp_path));
        return err;
    }

    ::sync();
    return Error::none;
}

// Parse /proc/mtd to find the MTD device by name
std::string find_mtd_device_by_name(std::string_view name) noexcept
{
    std::string mtd_content;
    if (posix_utils::read_file_to_string("/proc/mtd", mtd_content) != Error::none) {
        return {};
    }

    // Parse line by line: "mtdN: XXXXXXXX YYYYYYYY \"name\""
    std::string_view remaining(mtd_content);
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

        // Find quoted name at end of line
        const auto quote_end = line.rfind('"');
        if (quote_end == std::string_view::npos) continue;

        const auto quote_start = line.rfind('"', quote_end - 1);
        if (quote_start == std::string_view::npos) continue;

        const auto vol_name = line.substr(quote_start + 1, quote_end - quote_start - 1);
        if (vol_name != name) continue;

        // Extract mtdN from beginning of line
        const auto colon = line.find(':');
        if (colon == std::string_view::npos) continue;

        return std::string(line.substr(0, colon));
    }

    return {};
}

// Extract parent directory from path string
std::string parent_path(std::string_view path) noexcept
{
    const auto pos = path.rfind('/');
    if (pos == std::string_view::npos || pos == 0) {
        return "/";
    }
    return std::string(path.substr(0, pos));
}

} // anonymous namespace

std::string_view create_link::get_fw_env_config(PersistentMemDetector::MemType mem_type) noexcept
{
    if (mem_type == PersistentMemDetector::MemType::NAND) {
        return config::nand_uboot_env_path;
    }
    if (mem_type == PersistentMemDetector::MemType::eMMC) {
        return config::emmc_uboot_env_path;
    }
    return {};
}

std::string_view create_link::get_system_conf(PersistentMemDetector::MemType mem_type) noexcept
{
    if (mem_type == PersistentMemDetector::MemType::NAND) {
        return config::nand_rauc_system_conf_path;
    }
    if (mem_type == PersistentMemDetector::MemType::eMMC) {
        return config::emmc_rauc_system_conf_path;
    }
    return {};
}

Error create_link::create_link_to_system_conf(PersistentMemDetector::MemType type,
                                               std::string_view boot_device) noexcept
{
    const auto source = get_system_conf(type);
    if (source.empty()) {
        LOG_ERROR("memory type not defined for system.conf");
        return Error::invalid_argument;
    }

    const std::string destination(config::rauc_system_conf_path);

    if (!posix_utils::path_exists(destination)) {
        const Error err = posix_utils::copy_file(source, destination);
        if (err != Error::none) {
            LOG_ERROR("failed to copy system.conf");
            return err;
        }
    }

    if (type == PersistentMemDetector::MemType::eMMC) {
        if (!isBootDeviceConfigured(destination, boot_device)) {
            return update_mcc_boot_dev_conf(destination, boot_device);
        }
    }

    return Error::none;
}

Error create_link::create_link_to_fw_env_conf(PersistentMemDetector::MemType type,
                                               std::string_view boot_device) noexcept
{
    const auto source = get_fw_env_config(type);
    if (source.empty()) {
        LOG_ERROR("memory type not defined for fw_env.config");
        return Error::invalid_argument;
    }

    const std::string destination(config::uboot_env_path);
    const auto parent = parent_path(destination);

    if (!posix_utils::path_exists(parent)) {
        const Error err = posix_utils::mkdir_p(parent);
        if (err != Error::none) {
            return err;
        }
    }

    if (!posix_utils::path_exists(destination)) {
        const Error err = posix_utils::copy_file(source, destination);
        if (err != Error::none) {
            LOG_ERROR("failed to copy fw_env.config");
            return err;
        }
    }

    if (type == PersistentMemDetector::MemType::eMMC) {
        if (!isBootDeviceConfigured(destination, boot_device)) {
            return update_mcc_boot_dev_conf(destination, boot_device);
        }
    } else if (type == PersistentMemDetector::MemType::NAND) {
        const std::string mtd_device = find_mtd_device_by_name("UBootEnv");
        if (!mtd_device.empty() && !isBootDeviceConfigured(destination, mtd_device)) {
            return update_mtd_boot_dev_conf(destination, mtd_device);
        }
    }

    return Error::none;
}

bool create_link::isBootDeviceConfigured(std::string_view config_path,
                                          std::string_view expected_boot_device) noexcept
{
    std::string content;
    if (posix_utils::read_file_to_string(config_path, content) != Error::none) {
        return false;
    }

    const std::string expected_device_path = "/dev/" + std::string(expected_boot_device);

    std::string_view remaining(content);
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

        if (line.find("/dev/") != std::string_view::npos &&
            line.find(expected_device_path) != std::string_view::npos) {
            return true;
        }
    }

    return false;
}
