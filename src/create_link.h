#include <filesystem>
#include <exception>
#include "persistent_mem_detector.h"
#include "mount.h"

namespace create_link {

    class CreateRAMfsMountpoint : public std::exception
    {
        private:
            std::string error_msg;

        public:
            CreateRAMfsMountpoint(const std::filesystem::path &path)
            {
                this->error_msg = std::string("Error during creating mountpoint for RAMfs: ") + path.string();
            }

            const char * what() const throw () {
                return this->error_msg.c_str();
            }
    };

    class CreateSymlink : public std::exception
    {
        private:
            std::string error_msg;

        public:
            CreateSymlink(const std::filesystem::path &source, const std::filesystem::path &dest)
            {
                this->error_msg = std::string("Error during creating symlinks: from: ") + source.string()
                                + std::string(" to destination: ") + dest.string();
            }

            const char * what() const throw () {
                return this->error_msg.c_str();
            }
    };

    /**
     * Prepare ramdisk to create system internal "things" on filesystem. This will be created every boot and placed inside a ramdsik.
     * So it is independent of the existens of an application link in overlay.ini or a working persistent memory.
     * @param path_to_ramdisk Path to ramdisk in the "real" filesystem.
     * @param mem_type Current memory type of persistent filesystem.
     * @return ReadOnly object of overlay filesystem layer.
     * @throw CreateRAMfsMountpoint Can not create a ramdisk at given mountpoint.
     * @throw std::logic_error Can not determine persistent filesystem type.
     */
    OverlayDescription::ReadOnly prepare_ramdisk(const std::filesystem::path &, const PersistentMemDetector::MemType &);

    /**
     * Return "real" path of current UBoot-Environment path. 
     * @param mem_type Current memory type of persistent filesystem.
     * @return Path to matching UBoot-Environment configuration file.
     * @throw std::logic_error Can not determine persistent filesystem type.
     */
    std::filesystem::path get_fw_env_config(const PersistentMemDetector::MemType &);
};