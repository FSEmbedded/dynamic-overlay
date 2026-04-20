#pragma once

#include "config.h"
#include "error.h"

#include <string>
#include <string_view>

extern "C" {
    #include <sys/mount.h>
}

namespace OverlayDescription
{
    struct Persistent {
        std::string lower_directory;
        std::string work_directory;
        std::string merge_directory;
        std::string upper_directory;
        // Default-secure: upperdir is user-writable, so honoring setuid on
        // the merged view would turn any upperdir write into persistent root.
        // Opt out per-section in overlay.ini when the merge path legitimately
        // hosts setuid binaries from the signed lowerdir (e.g. /usr/bin).
        bool nosuid = true;

        bool operator==(const Persistent &other) const noexcept
        {
            return lower_directory == other.lower_directory &&
                   work_directory == other.work_directory &&
                   merge_directory == other.merge_directory &&
                   upper_directory == other.upper_directory &&
                   nosuid == other.nosuid;
        }

        bool operator!=(const Persistent &other) const noexcept
        {
            return !(*this == other);
        }
    };

    struct ReadOnly {
        std::string lower_directory;
        std::string merge_directory;

        bool operator==(const ReadOnly &other) const noexcept
        {
            return lower_directory == other.lower_directory &&
                   merge_directory == other.merge_directory;
        }

        bool operator!=(const ReadOnly &other) const noexcept
        {
            return !(*this == other);
        }
    };
}

class Mount
{
    const std::string path_to_container_;

    [[nodiscard]] bool is_mounted(std::string_view path) const noexcept;

public:
    Mount() noexcept;
    ~Mount() noexcept = default;

    Mount(const Mount &) = delete;
    Mount &operator=(const Mount &) = delete;
    Mount(Mount &&) = delete;
    Mount &operator=(Mount &&) = delete;

    [[nodiscard]] Error mount_application_image(std::string_view path_to_image) const noexcept;

    [[nodiscard]] Error mount_overlay_persistent(const OverlayDescription::Persistent &desc) const noexcept;

    [[nodiscard]] Error mount_overlay_readonly(const OverlayDescription::ReadOnly &desc) const noexcept;

    [[nodiscard]] Error wrapper_c_mount(std::string_view memory_device,
                                         std::string_view dest_dir,
                                         std::string_view options,
                                         std::string_view filesystem,
                                         unsigned long flag) noexcept;

    [[nodiscard]] Error wrapper_c_umount(std::string_view path) const noexcept;
};
