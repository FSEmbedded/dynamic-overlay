#pragma once

#include "error.h"
#include "mount.h"

#include <string>
#include <vector>

// Ordered mount/unmount for early boot virtual filesystems (/proc, /sys,
// persistent memory). Rollback unmounts in reverse order on failure.
namespace PreInit
{
    struct MountArgs {
        std::string source_dir;
        std::string dest_dir;
        std::string options;
        std::string filesystem_type;
        unsigned long flags = 0;
    };

    class PreInit
    {
        std::vector<std::string> mounted_paths_;
        std::vector<MountArgs> mount_prep_;

    public:
        PreInit() noexcept = default;
        ~PreInit() noexcept = default;

        PreInit(const PreInit &) = delete;
        PreInit &operator=(const PreInit &) = delete;
        PreInit(PreInit &&) = delete;
        PreInit &operator=(PreInit &&) = delete;

        void add(const MountArgs &args);

        [[nodiscard]] Error prepare() noexcept;

        [[nodiscard]] Error remove(const MountArgs &args) noexcept;
    };
}
