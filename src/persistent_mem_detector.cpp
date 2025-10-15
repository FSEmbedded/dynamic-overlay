#include "persistent_mem_detector.h"

#include <fstream>
#include <iostream>

#include <blkid/blkid.h> /* blkid functions */

namespace fs = std::filesystem; // Alias for filesystem

/* Use default values of regular expression,
 * if it is not definded in build process.
 * Use raw string literal to make expression simpler
*/
/* Regular expression to detect boot device (mmc) */
#ifndef PERSISTMEMORY_REGEX_EMMC
#define PERSISTMEMORY_REGEX_EMMC R"(root=\/dev\/(mmcblk[0-2]))"
#endif
/* Regular expression to detect boot device (nand) with ubifs.
 */
#ifndef PERSISTMEMORY_REGEX_NAND
#define PERSISTMEMORY_REGEX_NAND R"(root=\/dev\/(ubiblock\d+_\d+))"
#endif

/* Name of volume or partition of persitent memory */
#ifndef PERSISTMEMORY_DEVICE_NAME
#define PERSISTMEMORY_DEVICE_NAME "data"
#endif

#ifndef PERSISTENT_MEMORY_MOUNTPOINT
#define PERSISTENT_MEMORY_MOUNTPOINT "/rw_fs/root"
#endif

PersistentMemDetector::PersistentMemDetector::PersistentMemDetector()
    : nand_memory(PERSISTMEMORY_REGEX_NAND), emmc_memory(PERSISTMEMORY_REGEX_EMMC), mem_type(MemType::None),
    boot_device(""), path_to_mountpoint(PERSISTENT_MEMORY_MOUNTPOINT)
{
    std::ifstream bootdev("/sys/bdinfo/boot_dev");
    if (bootdev)
    {
        std::string bootdev_str;
        bootdev_str.reserve(16);

        if (std::getline(bootdev, bootdev_str))
        {
            // trim whitespace from both ends
            bootdev_str.erase(0, bootdev_str.find_first_not_of(" \t\r\n"));
            bootdev_str.erase(bootdev_str.find_last_not_of(" \t\r\n") + 1);
            // convert to lowercase
            std::transform(bootdev_str.begin(), bootdev_str.end(), bootdev_str.begin(),
                           [](unsigned char c)
                           { return std::tolower(c); });

            if (bootdev_str.find("nand") != std::string::npos)
            {
                this->mem_type = MemType::NAND;
                this->boot_device = bootdev_str;
                // continue to parse /proc/cmdline for exact device (ubiblockX_Y, etc.)
            }
            else
            {
                // mmc1 -> mmcblk0, mmc2 -> mmcblk1 mmc3 -> mmcblk2
                if (bootdev_str.find("mmc1") != std::string::npos)
                {
                    bootdev_str = "mmcblk0";
                }
                else if (bootdev_str.find("mmc2") != std::string::npos)
                {
                    bootdev_str = "mmcblk1";
                }
                else if (bootdev_str.find("mmc3") != std::string::npos)
                {
                    bootdev_str = "mmcblk2";
                }
                else
                {
                    throw(ErrorDeterminePersistentMemory());
                }
                this->mem_type = MemType::eMMC;
                this->boot_device = bootdev_str;
                return; // Successfully read the line
            }
        }
    }

    // Fallback to parsing /proc/cmdline if /sys/bdinfo/boot_dev is not available
    std::ifstream cmdline("/proc/cmdline");

    if (!cmdline) {
        throw ErrorOpenKernelParam("Cannot open /proc/cmdline");
    }

    std::string kernel_cmd;

    if (!std::getline(cmdline, kernel_cmd))
    {
        throw ErrorOpenKernelParam("Cannot read /proc/cmdline");
    }

    std::smatch device_match;

    if (std::regex_search(kernel_cmd, device_match, this->emmc_memory))
    {
        this->mem_type = MemType::eMMC;
        this->boot_device = device_match[1].str();
    }
    else if (std::regex_search(kernel_cmd, device_match, this->nand_memory))
    {
        this->mem_type = MemType::NAND;
        this->boot_device = device_match[1].str();
    }
    else
    {
        throw(ErrorDeterminePersistentMemory());
    }
}

PersistentMemDetector::PersistentMemDetector::~PersistentMemDetector()
{
}

PersistentMemDetector::MemType PersistentMemDetector::PersistentMemDetector::getMemType() const
{
    return this->mem_type;
}

std::string PersistentMemDetector::PersistentMemDetector::getPathToPersistentMemoryDevice(
    const std::shared_ptr<UBoot> &uboot_handler) const
{
    /* use device volume or partition name to find device */
    const char *label = PERSISTMEMORY_DEVICE_NAME;
    std::string storage_name;

    if (this->mem_type == MemType::eMMC)
    {
        blkid_cache cache = NULL;
        blkid_dev dev = NULL;
        if (blkid_get_cache(&cache, NULL) == 0)
        {
            dev = blkid_find_dev_with_tag(cache, "LABEL", label);
            if (dev)
            {
                const char *devname = blkid_dev_devname(dev);
                //std::cout << "Device name for label '" << label << "': " << devname << std::endl;
                return std::string(devname);
            }
        }
        storage_name = "Partition";
    }
    else if (this->mem_type == MemType::NAND)
    {
        /* is sysfs exists*/
        if(!std::filesystem::exists("/sys")) {
            throw std::runtime_error("sysfs is not mounted or /sys does not exist.");
        }

        if(!this->boot_device.empty())
        {
            // Extract UBI device number from boot_device (e.g., "ubiblock0_0" -> "0")
            std::string ubi_num;
            for (char c : this->boot_device) {
                if (std::isdigit(c)) {
                    ubi_num += c;
                } else if (!ubi_num.empty()) {
                    break;
                }
            }

            if (ubi_num.empty()) {
                std::cerr << "Cannot extract UBI device number from: " << this->boot_device << std::endl;
                throw ErrorDeterminePersistentMemory();
            }

            std::string ubi_dev = "ubi" + ubi_num;
            fs::path found_ubi_device_path = fs::path("/sys/class/ubi") / ubi_dev;

            if(std::filesystem::exists(found_ubi_device_path))
            {
                try
                {
                    // Iterate through all ubi0_X directories
                    for (const auto &entry : fs::directory_iterator(found_ubi_device_path))
                    {
                        std::string dirname = entry.path().filename().string();

                        // Filter: only ubi0_0, ubi0_1, ubi0_2, etc.
                        if (dirname.find(ubi_dev + "_") != 0)
                        {
                            continue;
                        }

                        fs::path name_file = entry.path() / "name";
                        if (!fs::exists(name_file))
                        {
                            continue;
                        }

                        // Read volume name
                        std::ifstream name_stream(name_file);
                        std::string vol_name;
                        if (!std::getline(name_stream, vol_name))
                        {
                            continue;
                        }

                        // Trim whitespace
                        auto trim_start = vol_name.find_first_not_of(" \t\n\r");
                        auto trim_end = vol_name.find_last_not_of(" \t\n\r");

                        if (trim_start != std::string::npos)
                        {
                            vol_name = vol_name.substr(trim_start, trim_end - trim_start + 1);
                        }

                        //std::cout << "Volume " << dirname << ": " << vol_name << std::endl;

                        if (vol_name == label)
                        {
                            std::string device = "/dev/" + dirname;
                            //std::cout << "Found UBI volume '" << label << "': " << device << std::endl;
                            return device;
                        }
                    }
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error: UBI volumes not found in sysfs." << std::endl;
                }
            }
        }
        storage_name = "Volume";
    }

    std::cerr << storage_name << " '" << label << "' not found." << std::endl;
    throw ErrorDeterminePersistentMemory();
}

std::string PersistentMemDetector::PersistentMemDetector::getBootDevice() const
{
    return this->boot_device;
}

std::filesystem::path PersistentMemDetector::PersistentMemDetector::getPathToPersistentMemoryDeviceMountPoint() const
{
    return this->path_to_mountpoint;
}
