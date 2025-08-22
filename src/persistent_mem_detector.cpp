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
#define PERSISTMEMORY_REGEX_NAND R"(root=\/dev\/(ubiblock[0-2]+))"
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
    if (bootdev.good())
    {
        std::string bootdev_str;
        std::getline(bootdev, bootdev_str);
        // convert to lowercase
        std::transform(bootdev_str.begin(), bootdev_str.end(), bootdev_str.begin(),
               [](unsigned char c){ return std::tolower(c); });

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
    }
    else
    {
        std::ifstream cmdline("/proc/cmdline");

        if (cmdline.good())
        {
            std::string kernel_cmd;
            std::getline(cmdline, kernel_cmd);
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
        else
        {
            if (cmdline.eof())
            {
                throw(ErrorOpenKernelParam("Empty file"));
            }
            else if (cmdline.fail())
            {
                throw(ErrorOpenKernelParam("Logical error on I/O operation"));
            }
            else if (cmdline.bad())
            {
                throw(ErrorOpenKernelParam("Read/writing error on I/O operation"));
            }
            else
            {
                throw(ErrorOpenKernelParam("Unknown"));
            }
        }
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
            std::string ubi_devices_path = "/sys/class/ubi";
            std::string found_ubi_device_path = ubi_devices_path + "/" + boot_device;

            if(std::filesystem::exists(found_ubi_device_path))
            {
                try
                {
                    std::ifstream count_file((found_ubi_device_path + "/volumes_count"));
                    int volumes_count = 0;
                    count_file >> volumes_count;
                    count_file.close();

                    // Loop through volume IDs and read their names
                    for (int volume_id = 0; volume_id < volumes_count; ++volume_id)
                    {
                        fs::path volume_name_path = volumes_count + "_" + std::to_string(volume_id) + "/name";

                        // Check if the path exists
                        if (fs::exists(volume_name_path))
                        {
                            std::ifstream volume_name_file(volume_name_path);
                            std::string volume_name;
                            volume_name_file >> volume_name;
                            volume_name_file.close();

                            //std::cout << "Volume " << volume_id << ": " << volume_name << std::endl;

                            if (volume_name == label)
                            {
                                //std::cout << "Volume '" << label << "' found!" << std::endl;
                                return volume_name;
                            }
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
