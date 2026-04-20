#include "mount.h"
#include "file_properties.h"
#include "posix_utils.h"
#include "logging.h"

#include <cerrno>
#include <cstring>
#include <string>

extern "C" {
#include <sys/mount.h>
#include <fcntl.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <unistd.h>
}

Mount::Mount() noexcept : path_to_container_(config::path_to_mount_appimage)
{
}

// Attach squashfs image via loop device and mount it at the container path.
// Loop device is acquired from /dev/loop-control and cleaned up on failure.
Error Mount::mount_application_image(std::string_view path_to_image) const noexcept
{
    const std::string image_path(path_to_image);

    FdGuard loopctl_fd(::open("/dev/loop-control", O_RDWR));
    if (!loopctl_fd.valid()) {
        LOG_ERRNO("open /dev/loop-control", errno);
        return Error::loop_device_failed;
    }

    const long devnr = ::ioctl(loopctl_fd.get(), LOOP_CTL_GET_FREE);
    if (devnr == -1) {
        LOG_ERRNO("ioctl LOOP_CTL_GET_FREE", errno);
        return Error::loop_device_failed;
    }

    const std::string loopname = "/dev/loop" + std::to_string(devnr);

    FdGuard loop_fd(::open(loopname.c_str(), O_RDWR));
    if (!loop_fd.valid()) {
        ::ioctl(loopctl_fd.get(), LOOP_CTL_REMOVE, devnr);
        LOG_ERRNO("open loop device", errno);
        return Error::loop_device_failed;
    }

    FdGuard backing_fd(::open(image_path.c_str(), O_RDWR));
    if (!backing_fd.valid()) {
        ::ioctl(loopctl_fd.get(), LOOP_CTL_REMOVE, devnr);
        LOG_ERRNO("open backing file", errno);
        return Error::loop_device_failed;
    }

    if (::ioctl(loop_fd.get(), LOOP_SET_FD, backing_fd.get()) == -1) {
        ::ioctl(loopctl_fd.get(), LOOP_CTL_REMOVE, devnr);
        LOG_ERRNO("ioctl LOOP_SET_FD", errno);
        return Error::loop_device_failed;
    }

    const int mount_state = ::mount(loopname.c_str(),
                                     path_to_container_.c_str(),
                                     "squashfs", 0, nullptr);
    if (mount_state != 0) {
        const int saved_errno = errno;
        ::ioctl(loop_fd.get(), LOOP_CLR_FD, 0);
        ::ioctl(loopctl_fd.get(), LOOP_CTL_REMOVE, devnr);
        LOG_ERRNO("mount squashfs", saved_errno);
        return Error::mount_failed;
    }

    return Error::none;
}

Error Mount::mount_overlay_persistent(const OverlayDescription::Persistent &container) const noexcept
{
    if (const Error err = posix_utils::mkdir_p(container.upper_directory); err != Error::none) {
        return err;
    }

    if (const Error err = posix_utils::mkdir_p(container.work_directory); err != Error::none) {
        return err;
    }

    if (!file_properties::properties_set(container)) {
        const Error props_err = file_properties::copy_properties_lower_to_upper(container);
        if (props_err != Error::none) {
            LOG_WARNING("failed to copy properties to upper directory");
        }
    }

    std::string mount_args;
    mount_args.reserve(256);
    mount_args.append("upperdir=");
    mount_args.append(container.upper_directory);
    mount_args.append(",workdir=");
    mount_args.append(container.work_directory);
    mount_args.append(",lowerdir=");
    mount_args.append(container.lower_directory);
    mount_args.append(",index=on,xino=auto");

    const unsigned long flags =
        static_cast<unsigned long>(MS_NODEV) |
        (container.nosuid ? static_cast<unsigned long>(MS_NOSUID) : 0UL);

    const int mount_state = ::mount("overlay",
                                     container.merge_directory.c_str(),
                                     "overlay", flags,
                                     mount_args.c_str());
    if (mount_state != 0) {
        LOG_ERRNO("mount overlay persistent", errno);
        return Error::overlay_mount_failed;
    }

    return Error::none;
}

Error Mount::mount_overlay_readonly(const OverlayDescription::ReadOnly &container) const noexcept
{
    if (is_mounted(container.merge_directory)) {
        LOG_INFO("found existing mount, attempting unmount: " + container.merge_directory);
        const Error err = wrapper_c_umount(container.merge_directory);
        if (err != Error::none) {
            LOG_WARNING("could not unmount existing mount at " + container.merge_directory);
            return err;
        }
    }

    std::string mount_args = "lowerdir=" + container.lower_directory + ",xino=auto";

    LOG_DEBUG("mounting read-only overlay: lowerdir=" + container.lower_directory +
              " merge=" + container.merge_directory);

    const int mount_state = ::mount("overlay",
                                     container.merge_directory.c_str(),
                                     "overlay", MS_RDONLY,
                                     mount_args.c_str());
    if (mount_state != 0) {
        LOG_ERRNO("mount overlay readonly", errno);
        return Error::overlay_mount_failed;
    }

    return Error::none;
}

Error Mount::wrapper_c_mount(std::string_view memory_device,
                              std::string_view dest_dir,
                              std::string_view options,
                              std::string_view filesystem,
                              unsigned long flag) noexcept
{
    const std::string dev(memory_device);
    const std::string dst(dest_dir);
    const std::string opts(options);
    const std::string fs(filesystem);

    const char *ptr_options = opts.empty() ? nullptr : opts.c_str();
    const char *ptr_fs = fs.empty() ? nullptr : fs.c_str();

    const int mount_state = ::mount(dev.c_str(), dst.c_str(), ptr_fs, flag, ptr_options);

    if (mount_state != 0) {
        LOG_ERRNO("mount " + dev + " on " + dst, errno);
        return Error::mount_failed;
    }

    return Error::none;
}

Error Mount::wrapper_c_umount(std::string_view path) const noexcept
{
    const std::string p(path);
    const int umount_state = ::umount(p.c_str());

    if (umount_state != 0) {
        LOG_ERRNO("umount " + p, errno);
        return Error::umount_failed;
    }

    return Error::none;
}

bool Mount::is_mounted(std::string_view path) const noexcept
{
    std::string mounts_content;
    if (posix_utils::read_file_to_string("/proc/mounts", mounts_content) != Error::none) {
        return false;
    }

    const std::string target = " " + std::string(path) + " ";
    return mounts_content.find(target) != std::string::npos;
}
