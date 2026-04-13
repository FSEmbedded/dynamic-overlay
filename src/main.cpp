#include "config.h"
#include "error.h"
#include "logging.h"
#include "posix_utils.h"
#include "u-boot.h"
#include "dynamic_mounting.h"
#include "preinit.h"
#include "persistent_mem_detector.h"
#include "create_link.h"

#include <string>

extern "C" {
#include <sys/mount.h>
}

#ifdef BUILD_X509_CERTIFICATE_STORE_MOUNT
    #include "x509_cert_store.h"
    #include "mount.h"

extern "C" {
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
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

[[nodiscard]] Error chown_recursive(std::string_view path, uid_t uid, gid_t gid) noexcept
{
    const std::string p(path);

    if (::lchown(p.c_str(), uid, gid) != 0) {
        LOG_ERRNO("chown_recursive: lchown failed", errno);
        return Error::chown_failed;
    }

    struct stat st {};
    if (::lstat(p.c_str(), &st) != 0) {
        return Error::stat_failed;
    }

    if (!S_ISDIR(st.st_mode)) {
        return Error::none;
    }

    DirGuard dir(::opendir(p.c_str()));
    if (!dir.valid()) {
        LOG_ERRNO("chown_recursive: opendir failed", errno);
        return Error::open_failed;
    }

    struct dirent *entry = nullptr;
    while ((entry = ::readdir(dir.get())) != nullptr) {
        const std::string_view name(entry->d_name);
        if (name == "." || name == "..") continue;

        const std::string child = p + "/" + std::string(name);
        const Error err = chown_recursive(child, uid, gid);
        if (err != Error::none) {
            LOG_WARNING("chown_recursive: failed for " + child);
        }
    }

    return Error::none;
}

} // anonymous namespace
#endif

// Preinit entry point — runs before any init system.
// Boot sequence: /proc+/sys → detect storage → mount persistent →
// create config links → mount overlays → (optional: cert store) →
// unmount /proc+/sys → exec init.
int main()
{
    // Stage 1: mount /proc and /sys
    PreInit::MountArgs proc_args;
    proc_args.source_dir = "proc";
    proc_args.dest_dir = "/proc";
    proc_args.filesystem_type = "proc";
    proc_args.flags = MS_NOSUID | MS_NOEXEC | MS_NODEV;

    PreInit::MountArgs sys_args;
    sys_args.source_dir = "sys";
    sys_args.dest_dir = "/sys";
    sys_args.filesystem_type = "sysfs";
    sys_args.flags = MS_NOSUID | MS_NOEXEC | MS_NODEV;

    PreInit::PreInit init_stage1;
    init_stage1.add(proc_args);
    init_stage1.add(sys_args);

    Error err = init_stage1.prepare();
    if (err != Error::none) {
        LOG_FATAL("failed to mount /proc and /sys");
        return 1;
    }

    // Detect persistent memory
    PersistentMemDetector::PersistentMemDetector mem_dect;
    err = PersistentMemDetector::PersistentMemDetector::create(mem_dect);
    if (err != Error::none) {
        LOG_ERROR("failed to detect persistent memory: " + std::string(error_to_string(err)));
    }

    // Create U-Boot handler on stack
    UBoot uboot(config::uboot_env_path);

    // Stage 2: mount persistent memory
    if (err == Error::none) {
        PreInit::MountArgs persistent_args;
        std::string persistent_device;
        err = mem_dect.getPathToPersistentMemoryDevice(uboot, persistent_device);
        if (err == Error::none) {
            persistent_args.source_dir = persistent_device;
            persistent_args.dest_dir = std::string(mem_dect.getPathToPersistentMemoryDeviceMountPoint());
            persistent_args.flags = 0;

            if (mem_dect.getMemType() == PersistentMemDetector::MemType::eMMC) {
                persistent_args.filesystem_type = "ext4";
            } else if (mem_dect.getMemType() == PersistentMemDetector::MemType::NAND) {
                persistent_args.filesystem_type = "ubifs";
            } else {
                LOG_ERROR("could not determine memory type (NAND|eMMC)");
                err = Error::memory_detect_failed;
            }

            if (err == Error::none) {
                PreInit::PreInit init_stage2;
                init_stage2.add(persistent_args);
                err = init_stage2.prepare();
                if (err != Error::none) {
                    LOG_ERROR("failed to mount persistent memory");
                }
            }
        } else {
            LOG_ERROR("failed to get persistent memory device path");
        }
    }

    // Create config links
    if (mem_dect.getMemType() != PersistentMemDetector::MemType::None) {
        static_cast<void>(create_link::create_link_to_system_conf(
            mem_dect.getMemType(), mem_dect.getBootDevice()));
        static_cast<void>(create_link::create_link_to_fw_env_conf(
            mem_dect.getMemType(), mem_dect.getBootDevice()));
    }

    // Dynamic mounting
    Error mount_error = Error::none;
    {
        DynamicMounting handler(uboot);
        mount_error = handler.application_image();
    }

#ifdef BUILD_X509_CERTIFICATE_STORE_MOUNT
    // X.509 cert store: mount tmpfs on target dir, extract certs, freeze readonly
    // Runs after application_image() so /etc overlay is available
    {
        const std::string cert_dir(config::target_archiv_dir_path);
        Mount cert_mount;
        bool cert_tmpfs_mounted = false;

        Error cert_err = posix_utils::mkdir_p(cert_dir);
        if (cert_err == Error::none) {
            cert_err = cert_mount.wrapper_c_mount("none", cert_dir, "size=1M", "tmpfs", 0);
            if (cert_err == Error::none) {
                cert_tmpfs_mounted = true;
            }
        }

        if (cert_err == Error::none) {
            if (mem_dect.getMemType() == PersistentMemDetector::MemType::eMMC) {
                x509_store::CertMMCstore cert_store;
                cert_err = cert_store.ExtractCertStore(mem_dect.getBootDevice());
            } else if (mem_dect.getMemType() == PersistentMemDetector::MemType::NAND) {
                x509_store::CertMDTstore cert_store;
                cert_err = cert_store.ExtractCertStore();
            }
        }

        if (cert_err == Error::none) {
            struct passwd *pwd = ::getpwnam("adu");
            struct group *grp = ::getgrnam("adu");
            if (pwd && grp) {
                static_cast<void>(chown_recursive(cert_dir, pwd->pw_uid, grp->gr_gid));
            } else {
                LOG_WARNING("user/group 'adu' not found");
            }

            // Freeze: remount readonly
            static_cast<void>(cert_mount.wrapper_c_mount(
                "none", cert_dir, "", "tmpfs", MS_REMOUNT | MS_RDONLY));
        } else if (cert_tmpfs_mounted) {
            LOG_WARNING("cert store failed: " + std::string(error_to_string(cert_err)));
            static_cast<void>(cert_mount.wrapper_c_umount(cert_dir));
        }
    }
#endif

    // Cleanup: unmount /sys and /proc
    static_cast<void>(init_stage1.remove(sys_args));
    static_cast<void>(init_stage1.remove(proc_args));

    if (mount_error != Error::none) {
        LOG_ERROR("error during dynamic mounting: " + std::string(error_to_string(mount_error)));
    }

    return 0;
}
