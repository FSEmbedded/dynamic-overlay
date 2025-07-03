#include <filesystem>
#include <exception>
#include "persistent_mem_detector.h"
#include "mount.h"

namespace create_link
{
    class CreateSymlink : public std::exception
    {
    private:
        std::string error_msg;

    public:
        CreateSymlink(const std::filesystem::path &source, const std::filesystem::path &dest)
        {
            this->error_msg = std::string("Error during creating symlinks: from: ") + source.string() + std::string(" to destination: ") + dest.string();
        }

        const char *what() const throw()
        {
            return this->error_msg.c_str();
        }
    };

    /**
     * Get "real" path of current UBoot-Environment path.
     * @param mem_type Current memory type of persistent filesystem.
     * @return Path to matching UBoot-Environment configuration file.
     * @throw std::logic_error Can not determine persistent filesystem type.
     */
    std::filesystem::path get_fw_env_config(const PersistentMemDetector::MemType &);

    /**
     * Get "real" path of current RAUC system configuration file.
     * @param type Current memory type of persistent filesystem.
     * @return Path to matching RAUC system configuration file.
     * @throw std::logic_error Can not determine persistent filesystem type.
     */
    std::filesystem::path get_system_conf(const PersistentMemDetector::MemType &type);

    /**
     * Creates a soft link to the system_conf file depending on the board memory type.
     * This file is used by RAUC.
     */
    void create_link_to_system_conf(const PersistentMemDetector::MemType &type, const std::string &boot_device);

    /**
     * Creates a soft link to the fw_env_conf file depending on the board memory type.
     * This file is used by system tools like fw_printenv.
     */
    void create_link_to_fw_env_conf(const PersistentMemDetector::MemType &type, const std::string &boot_device);

    /**
     * Checks if the boot device is configured in the fw_env.conf file.
     * @param config_path Path to the config file.
     * @param expected_boot_device Expected boot device string.
     * @return True if the boot device is configured, false otherwise.
     */
    bool isBootDeviceConfigured(const std::filesystem::path& config_path, const std::string& expected_boot_device);
};
