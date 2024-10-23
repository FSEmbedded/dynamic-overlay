#include "persistent_mem_detector.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <blkid/blkid.h> /* blkid functions */

namespace fs = std::filesystem; // Alias for filesystem

/* Use default values of regular expression,
 * if it is not definded in build process.
*/
/* Regular expression to detect boot device (mmc) */
#ifndef PERSISTMEMORY_REGEX_EMMC
#define PERSISTMEMORY_REGEX_EMMC "root=/dev/mmcblk[0-2]p[0-9]{1,3}"
#endif
/* Regular expression to detect boot device (nand) with ubifs.
 */
#ifndef PERSISTMEMORY_REGEX_NAND
#define PERSISTMEMORY_REGEX_NAND "root=/dev/ubiblock0_[0-1]"
#endif

/* Name of volume or partition of persitent memory */
#ifndef PERSISTMEMORY_DEVICE_NAME
#define PERSISTMEMORY_DEVICE_NAME "data"
#endif

PersistentMemDetector::PersistentMemDetector::PersistentMemDetector()
    : nand_memory(PERSISTMEMORY_REGEX_NAND), emmc_memory(PERSISTMEMORY_REGEX_EMMC), mem_type(MemType::None)
{
    std::ifstream cmdline("/proc/cmdline");

    if (cmdline.good())
    {
        std::string kernel_cmd;
        std::getline(cmdline, kernel_cmd);

        if (std::regex_search(kernel_cmd, this->emmc_memory))
        {
            this->mem_type = MemType::eMMC;
        }
        else if (std::regex_search(kernel_cmd, this->nand_memory))
        {
            this->mem_type = MemType::NAND;
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
        try
        {
            std::ifstream count_file("/sys/class/ubi/ubi0/volumes_count");
            int volumes_count = 0;
            count_file >> volumes_count;
            count_file.close();

            // Loop through volume IDs and read their names
            for (int volume_id = 0; volume_id < volumes_count; ++volume_id)
            {
                fs::path volume_name_path = "/sys/class/ubi/ubi0_" + std::to_string(volume_id) + "/name";

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
        storage_name = "Volume";
    }

    std::cerr << storage_name << " '" << label << "' not found." << std::endl;
    throw ErrorDeterminePersistentMemory();
}
