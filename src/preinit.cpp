#include "preinit.h"
#include "logging.h"

#include <algorithm>

void PreInit::PreInit::add(const MountArgs &handler)
{
    mount_prep_.push_back(handler);
}

Error PreInit::PreInit::prepare() noexcept
{
    Mount mount_handler;

    for (auto &entry : mount_prep_) {
        const Error err = mount_handler.wrapper_c_mount(
            entry.source_dir,
            entry.dest_dir,
            entry.options,
            entry.filesystem_type,
            entry.flags
        );

        if (err != Error::none) {
            LOG_ERROR("prepare: mount failed for " + entry.dest_dir);
            // Rollback already-mounted paths in reverse order
            for (auto it = mounted_paths_.rbegin(); it != mounted_paths_.rend(); ++it) {
                static_cast<void>(mount_handler.wrapper_c_umount(*it));
            }
            mounted_paths_.clear();
            return err;
        }
        mounted_paths_.push_back(entry.dest_dir);
    }

    return Error::none;
}

Error PreInit::PreInit::remove(const MountArgs &obj) noexcept
{
    auto it = std::find(mounted_paths_.begin(), mounted_paths_.end(), obj.dest_dir);
    if (it == mounted_paths_.end()) {
        LOG_ERROR("remove: path not found in mounted list: " + obj.dest_dir);
        return Error::not_mounted;
    }

    Mount mount_handler;
    const Error err = mount_handler.wrapper_c_umount(obj.dest_dir);
    if (err != Error::none) {
        return err;
    }

    mounted_paths_.erase(it);
    return Error::none;
}
