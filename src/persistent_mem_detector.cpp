#include "persistent_mem_detector.h"

#include <fstream>
#include <filesystem>

PersistentMemDetector::PersistentMemDetector::PersistentMemDetector():
    nand_memory("root=/dev/ubiblock0_[0-1]"),
    emmc_memory("root=/dev/mmcblk2p[7-8]"),
    mem_type(MemType::None)
{
    std::ifstream cmdline("/proc/cmdline");

    if(cmdline.good())
    {
        std::string kernel_cmd;
        std::getline(cmdline, kernel_cmd);

        if(std::regex_search(kernel_cmd, this->emmc_memory))
        {
            this->mem_type = MemType::eMMC;
        }
        else if(std::regex_search(kernel_cmd, this->nand_memory))
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
        if(cmdline.eof())
        {
            throw(ErrorOpenKernelParam("Empty file"));
        }
        else if(cmdline.fail())
        {
            throw(ErrorOpenKernelParam("Logical error on I/O operation"));
        }
        else if(cmdline.bad())
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

std::string PersistentMemDetector::PersistentMemDetector::getPathToPersistentMemoryDevice(const std::shared_ptr<UBoot> &uboot_handler) const
{
    if (this->mem_type == MemType::eMMC)
    {
		const std::string mmcdev = uboot_handler->getVariable("mmcdev", std::vector<std::string>({"0", "1", "2"}));
        return std::string(std::string("/dev/mmcblk") + mmcdev + std::string("p9"));
    }
    else if (this->mem_type == MemType::NAND)
    {
        return std::string("/dev/ubi0_2");
    }
    else
    {
        throw ErrorDeterminePersistentMemory();
    }
}

