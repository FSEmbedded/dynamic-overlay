#include "persistent_mem_detector.h"
#include "config.h"
#include "posix_utils.h"
#include "string_utils.h"
#include "logging.h"

#include <cerrno>
#include <cstring>

extern "C" {
#include <dirent.h>
#include <blkid/blkid.h>
}

namespace {

class DirGuard {
    DIR *dir_ = nullptr;

public:
    explicit DirGuard(DIR *d) noexcept : dir_(d) {}
    ~DirGuard() noexcept { if (dir_) ::closedir(dir_); }

    DirGuard(const DirGuard &) = delete;
    DirGuard &operator=(const DirGuard &) = delete;

    [[nodiscard]] DIR *get() const noexcept { return dir_; }
    [[nodiscard]] bool valid() const noexcept { return dir_ != nullptr; }
};

class BlkidCacheGuard {
    blkid_cache cache_ = nullptr;

public:
    BlkidCacheGuard() noexcept = default;
    ~BlkidCacheGuard() noexcept { if (cache_) blkid_put_cache(cache_); }

    BlkidCacheGuard(const BlkidCacheGuard &) = delete;
    BlkidCacheGuard &operator=(const BlkidCacheGuard &) = delete;

    [[nodiscard]] Error init() noexcept
    {
        if (blkid_get_cache(&cache_, nullptr) != 0) {
            return Error::blkid_failed;
        }
        return Error::none;
    }

    [[nodiscard]] blkid_cache get() const noexcept { return cache_; }
};

} // anonymous namespace

Error PersistentMemDetector::PersistentMemDetector::detect_from_bdinfo() noexcept
{
    std::string bootdev_str;
    const Error err = posix_utils::read_file_to_string("/sys/bdinfo/boot_dev", bootdev_str);
    if (err != Error::none) {
        return err;
    }

    const auto trimmed = string_utils::trim(bootdev_str);
    const auto lower = string_utils::to_lower(trimmed);

    if (lower.find("nand") != std::string::npos) {
        mem_type_ = MemType::NAND;
        boot_device_ = lower;
        return Error::none;
    }

    if (lower.find("mmc1") != std::string::npos) {
        boot_device_ = "mmcblk0";
    } else if (lower.find("mmc2") != std::string::npos) {
        boot_device_ = "mmcblk1";
    } else if (lower.find("mmc3") != std::string::npos) {
        boot_device_ = "mmcblk2";
    } else {
        return Error::memory_detect_failed;
    }

    mem_type_ = MemType::eMMC;
    return Error::none;
}

Error PersistentMemDetector::PersistentMemDetector::detect_from_cmdline() noexcept
{
    std::string kernel_cmd;
    const Error err = posix_utils::read_file_to_string("/proc/cmdline", kernel_cmd);
    if (err != Error::none) {
        LOG_ERROR("cannot read /proc/cmdline");
        return Error::memory_detect_failed;
    }

    std::string device;
    if (detail::parse_emmc_device(kernel_cmd, device)) {
        mem_type_ = MemType::eMMC;
        boot_device_ = device;
        return Error::none;
    }

    if (detail::parse_nand_device(kernel_cmd, device)) {
        mem_type_ = MemType::NAND;
        boot_device_ = device;
        return Error::none;
    }

    LOG_ERROR("persistent memory could not be determined from cmdline");
    return Error::memory_detect_failed;
}

// Detection order: /sys/bdinfo (vendor sysfs) first, /proc/cmdline fallback.
// For NAND via bdinfo, cmdline is still needed for the exact ubiblock device.
Error PersistentMemDetector::PersistentMemDetector::create(PersistentMemDetector &out) noexcept
{
    out.path_to_mountpoint_ = config::persistent_memory_mountpoint;
    out.mem_type_ = MemType::None;

    // Try bdinfo first
    Error err = out.detect_from_bdinfo();
    if (err == Error::none) {
        // For NAND detected via bdinfo, we still need cmdline for exact device
        if (out.mem_type_ == MemType::NAND) {
            // boot_device_ has bdinfo value, try to get exact ubiblock from cmdline
            std::string kernel_cmd;
            if (posix_utils::read_file_to_string("/proc/cmdline", kernel_cmd) == Error::none) {
                std::string nand_dev;
                if (detail::parse_nand_device(kernel_cmd, nand_dev)) {
                    out.boot_device_ = nand_dev;
                }
            }
        }
        return Error::none;
    }

    // Fallback to cmdline parsing
    return out.detect_from_cmdline();
}

PersistentMemDetector::MemType
PersistentMemDetector::PersistentMemDetector::getMemType() const noexcept
{
    return mem_type_;
}

Error PersistentMemDetector::PersistentMemDetector::getPathToPersistentMemoryDevice(
    const UBoot & /*uboot*/, std::string &out) const noexcept
{
    const char *label = config::persistmemory_device_name;

    if (mem_type_ == MemType::eMMC) {
        BlkidCacheGuard cache;
        if (cache.init() != Error::none) {
            LOG_ERROR("blkid cache init failed");
            return Error::blkid_failed;
        }

        blkid_dev dev = blkid_find_dev_with_tag(cache.get(), "LABEL", label);
        if (dev != nullptr) {
            const char *devname = blkid_dev_devname(dev);
            if (devname != nullptr) {
                out = std::string(devname);
                return Error::none;
            }
        }

        LOG_ERROR(std::string("partition '") + label + "' not found");
        return Error::memory_detect_failed;
    }

    if (mem_type_ == MemType::NAND) {
        if (!posix_utils::path_exists("/sys")) {
            LOG_ERROR("sysfs not mounted");
            return Error::memory_detect_failed;
        }

        if (boot_device_.empty()) {
            LOG_ERROR("boot device not set for NAND");
            return Error::memory_detect_failed;
        }

        // Extract UBI device number from boot_device (e.g., "ubiblock0_0" -> "0")
        std::string ubi_num;
        for (char c : boot_device_) {
            if (std::isdigit(static_cast<unsigned char>(c))) {
                ubi_num += c;
            } else if (!ubi_num.empty()) {
                break;
            }
        }

        if (ubi_num.empty()) {
            LOG_ERROR("cannot extract UBI device number from: " + boot_device_);
            return Error::memory_detect_failed;
        }

        const std::string ubi_dev = "ubi" + ubi_num;
        const std::string ubi_path = "/sys/class/ubi/" + ubi_dev;

        if (!posix_utils::path_exists(ubi_path)) {
            LOG_ERROR("UBI device path not found: " + ubi_path);
            return Error::memory_detect_failed;
        }

        DirGuard dir(::opendir(ubi_path.c_str()));
        if (!dir.valid()) {
            LOG_ERRNO("opendir failed", errno);
            return Error::memory_detect_failed;
        }

        const std::string prefix = ubi_dev + "_";
        struct dirent *entry = nullptr;
        while ((entry = ::readdir(dir.get())) != nullptr) {
            const std::string_view dirname(entry->d_name);
            if (dirname.substr(0, prefix.size()) != prefix) {
                continue;
            }

            const std::string name_file = ubi_path + "/" + std::string(dirname) + "/name";
            std::string vol_name;
            if (posix_utils::read_file_to_string(name_file, vol_name) != Error::none) {
                continue;
            }

            // Trim whitespace
            const auto sv = string_utils::trim(std::string_view(vol_name));
            if (sv == label) {
                out = "/dev/" + std::string(dirname);
                return Error::none;
            }
        }

        LOG_ERROR(std::string("UBI volume '") + label + "' not found");
        return Error::memory_detect_failed;
    }

    LOG_ERROR("memory type not determined");
    return Error::memory_detect_failed;
}

std::string_view PersistentMemDetector::PersistentMemDetector::getBootDevice() const noexcept
{
    return boot_device_;
}

std::string_view PersistentMemDetector::PersistentMemDetector::getPathToPersistentMemoryDeviceMountPoint() const noexcept
{
    return path_to_mountpoint_;
}
