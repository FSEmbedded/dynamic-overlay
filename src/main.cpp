#include "u-boot.h"
#include "dynamic_mounting.h"
#include "preinit.h"
#include "persistent_mem_detector.h"
#include "create_link.h"

#ifdef BUILD_X509_CERTIFICATE_STORE_MOUNT
    #include "x509_cert_store.h"
#endif

#include <iostream>
#include <string>
#include <memory>

#include <unistd.h>

#include <sys/types.h>   // uid_t, gid_t
#include <sys/stat.h>    // for struct stat und stat()-Funktionen
#include <sys/wait.h>    // for waitpid() und WIFEXITED, WEXITSTATUS
#include <pwd.h>         // for getpwnam() und struct passwd
#include <grp.h>         // for getgrnam() und struct group
#include <errno.h>       // for errno und Fehlercodes

int main()
{
    try
    {
        PreInit::MountArgs proc = PreInit::MountArgs();
        proc.source_dir = std::string("proc");
        proc.dest_dir = std::string("/proc");
        proc.filesystem_type = "proc";
        proc.flags = MS_NOSUID | MS_NOEXEC | MS_NODEV;

        PreInit::MountArgs sys = PreInit::MountArgs();
        sys.source_dir = std::string("sys");
        sys.dest_dir = std::string("/sys");
        sys.filesystem_type = "sysfs";
        sys.flags = MS_NOSUID | MS_NOEXEC | MS_NODEV;

        PreInit::MountArgs persistent = PreInit::MountArgs();

        OverlayDescription::ReadOnly ramdisk;

        PreInit::PreInit init_stage1 = PreInit::PreInit();

        /* First mount proc and then sysfs. This ensures
         * necessary system info is available before starting
         * other filesystems and services.
        */
        init_stage1.add(proc);
        init_stage1.add(sys);
        init_stage1.prepare();

        PersistentMemDetector::PersistentMemDetector mem_dect;
        std::shared_ptr<UBoot> uboot = std::make_shared<UBoot>(std::string("/etc/fw_env.config"));

        std::exception_ptr error_during_mount_persistent;
        try
        {
            PreInit::PreInit init_stage2 = PreInit::PreInit();
            persistent.source_dir = mem_dect.getPathToPersistentMemoryDevice(uboot);
            persistent.dest_dir = mem_dect.getPathToPersistentMemoryDeviceMountPoint();
            persistent.flags = 0;
            if (mem_dect.getMemType() == PersistentMemDetector::MemType::eMMC)
            {
                persistent.filesystem_type = "ext4";
            }
            else if (mem_dect.getMemType() == PersistentMemDetector::MemType::NAND)
            {
                persistent.filesystem_type = "ubifs";
            }
            else
            {
                throw(std::logic_error("Could not determine current memory type (NAND|eMMC)"));
            }
            init_stage2.add(persistent);
            init_stage2.prepare();
        }
        catch (const std::exception &err)
        {
            std::cerr << "dynamicoverlay: Error during mount persistent memory: " << err.what() << std::endl;
        }

        create_link::create_link_to_system_conf(mem_dect.getMemType(), mem_dect.getBootDevice());
        create_link::create_link_to_fw_env_conf(mem_dect.getMemType(), mem_dect.getBootDevice());

        try
        {
            // The overlay link is not updated in this scope. It must use "the old" path.
            DynamicMounting handler(uboot);
#ifdef BUILD_X509_CERTIFICATE_STORE_MOUNT
            OverlayDescription::ReadOnly ramdisk_x509_unpacked_store;
            x509_store::prepare_ramdisk_readable(RAMFS_CERT_STORE_MOUNTPOINT);
            bool handle_secure_store_fails = false;
            /* Handling to extract secure store is not critical.
            *  Handle exception from store as warnings.
            */
            try {

                if (mem_dect.getMemType() == PersistentMemDetector::MemType::eMMC)
                {
                    x509_store::CertMMCstore cert_store;
                    cert_store.ExtractCertStore(RAMFS_CERT_STORE_MOUNTPOINT, mem_dect.getBootDevice());
                }
                else if (mem_dect.getMemType() == PersistentMemDetector::MemType::NAND)
                {
                    x509_store::CertMDTstore cert_store;
                    cert_store.ExtractCertStore(RAMFS_CERT_STORE_MOUNTPOINT);
                }

                /* after installation prepare for permissions */
                int ret = ::system("chown -R adu:adu /adu");
                if (ret == -1) {
                    std::cerr << "dynamicoverlay: Error executing chown command: " << strerror(errno) << std::endl;
                    throw std::runtime_error("Failed to execute chown command");
                } else if (ret == 127) {
                    std::cerr << "dynamicoverlay: Error: Shell could not execute chown command" << std::endl;
                    throw std::runtime_error("Shell execution failed");
                } else if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0) {
                    std::cerr << "dynamicoverlay: Warning: chown command exited with status "
                              << WEXITSTATUS(ret) << std::endl;
                }

                struct stat dir_stat;
                if (stat("/adu", &dir_stat) == 0) {
                    // Get UID und GID for test only
                    struct passwd *pwd = getpwnam("adu");
                    struct group *grp = getgrnam("adu");

                    if (pwd && grp && (dir_stat.st_uid != pwd->pw_uid || dir_stat.st_gid != grp->gr_gid)) {
                        std::cerr << "Warning: Directory permissions were not set correctly" << std::endl;
                    }
                }

                ramdisk_x509_unpacked_store = x509_store::prepare_readonly_overlay_from_ramdisk(RAMFS_CERT_STORE_MOUNTPOINT);

                handler.add_lower_dir_readonly_memory(ramdisk_x509_unpacked_store);
            } catch(std::exception const& ex) {
                std::cerr << "dynamicoverlay: Warning, " << ex.what() << std::endl;
            }  catch(int e) {
                std::cerr << "dynamicoverlay: Warning, directory adu can't change own %d" << e << std::endl;
            }

            if(handle_secure_store_fails == true)
                x509_store::close_ramdisk(RAMFS_CERT_STORE_MOUNTPOINT);
#endif
            handler.application_image();
        }
        catch(...)
        {
            error_during_mount_persistent = std::current_exception();
        }

        init_stage1.remove(sys);
        init_stage1.remove(proc);

        if(error_during_mount_persistent)
        {
            std::rethrow_exception(error_during_mount_persistent);
        }
    }
    catch (const std::exception &err)
    {
        std::cerr << "dynamicoverlay: Error during execution: " << err.what() << std::endl;
    }

    return 0;
}
