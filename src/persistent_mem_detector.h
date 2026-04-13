#pragma once

#include "device_parser.h"
#include "error.h"
#include "u-boot.h"

#include <memory>
#include <string>
#include <string_view>

namespace PersistentMemDetector
{
    enum class MemType : uint8_t {
        eMMC,
        NAND,
        None
    };

    class PersistentMemDetector
    {
        MemType mem_type_ = MemType::None;
        std::string boot_device_;
        std::string path_to_mountpoint_;

        [[nodiscard]] Error detect_from_bdinfo() noexcept;
        [[nodiscard]] Error detect_from_cmdline() noexcept;

    public:
        PersistentMemDetector() noexcept = default;
        ~PersistentMemDetector() noexcept = default;

        PersistentMemDetector(const PersistentMemDetector &) = delete;
        PersistentMemDetector &operator=(const PersistentMemDetector &) = delete;
        PersistentMemDetector(PersistentMemDetector &&) = delete;
        PersistentMemDetector &operator=(PersistentMemDetector &&) = delete;

        [[nodiscard]] static Error create(PersistentMemDetector &out) noexcept;

        [[nodiscard]] MemType getMemType() const noexcept;

        [[nodiscard]] Error getPathToPersistentMemoryDevice(const UBoot &uboot,
                                                             std::string &out) const noexcept;

        [[nodiscard]] std::string_view getBootDevice() const noexcept;

        [[nodiscard]] std::string_view getPathToPersistentMemoryDeviceMountPoint() const noexcept;
    };
}
