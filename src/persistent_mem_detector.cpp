#include "persistent_mem_detector.h"

#include <fstream>
#include <filesystem>

PersistentMemDetector::PersistentMemDetector::PersistentMemDetector():
    nand_memory("root=/dev/ubiblock0_[0-1]"),
    emmc_memory("root=/dev/mmcblk2p[7-8]")
{
    
}

PersistentMemDetector::PersistentMemDetector::~PersistentMemDetector()
{

}

PersistentMemDetector::MemType PersistentMemDetector::PersistentMemDetector::getMemType()
{
    std::ifstream cmdline("/proc/cmdline");

    if(cmdline.good())
    {
        std::string kernel_cmd;
        std::getline(cmdline, kernel_cmd);

        if(std::regex_search(kernel_cmd, this->emmc_memory))
        {
            return MemType::eMMC;
        }
        else if(std::regex_search(kernel_cmd, this->nand_memory))
        {
            return MemType::NAND;
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

    return MemType::None;
}
