#pragma once

#include "u-boot.h"

#include <regex>
#include <exception>
#include <string>
#include <memory>
#include <filesystem>

/**
 * Helper functionality to detect the persistent memory type.
 *
 * The problem is, that multiple types of memory exists for the different F&S boards.
 * This class helps to detect the right one.
 */
namespace PersistentMemDetector
{
    enum class MemType
    {
        eMMC,
        NAND,
        None
    };

    //////////////////////////////////////////////////////////////////////////////
    // Own Exceptions

    class ErrorOpenKernelParam : public std::exception
    {
    private:
        std::string error_msg;

    public:
        /**
         * Can not open commandline information. This may a not mounted /proc filesystem.
         * @param msg Path to commandline argument.
         */
        explicit ErrorOpenKernelParam(const std::string &msg)
        {
            this->error_msg = std::string("Could not open kernel commandline parameters: ") + msg;
        }
        const char *what() const throw()
        {
            return this->error_msg.c_str();
        }
    };

    class ErrorDeterminePersistentMemory : public std::exception
    {
    private:
        std::string error_msg;

    public:
        /**
         * Can not get persistent memory.
         */
        ErrorDeterminePersistentMemory()
        {
            this->error_msg = "Persistent memory could not be determined";
        }
        const char *what() const throw()
        {
            return this->error_msg.c_str();
        }
    };

    //////////////////////////////////////////////////////////////////////////////

    class PersistentMemDetector
    {
    private:
        std::regex nand_memory;
        std::regex emmc_memory;
        MemType mem_type;
        std::string boot_device;
        std::filesystem::path path_to_mountpoint;

    public:
        /**
         * Set regex detection for NAND and eMMC.
         * @throw ErrorOpenKernelParam
         * @throw ErrorDeterminePersistentMemory
         */
        PersistentMemDetector();
        ~PersistentMemDetector();

        PersistentMemDetector(const PersistentMemDetector &) = delete;
        PersistentMemDetector &operator=(const PersistentMemDetector &) = delete;
        PersistentMemDetector(PersistentMemDetector &&) = delete;
        PersistentMemDetector &operator=(PersistentMemDetector &&) = delete;

        /**
         * Get memory type of booted system to determine current persistent memory type for mounting.
         * @return MemType of current memory type.
         */
        MemType getMemType() const;

        /**
         * Get Path to persistent memory device
         * @return String of path to memory device.
         */
        std::string getPathToPersistentMemoryDevice(const std::shared_ptr<UBoot> &) const;

        /**
         * Get boot device.
         * @return String of boot device.
         */
        std::string getBootDevice() const;
        /*
         * Get path to persistent memory device mount point.
         * @return Path to persistent memory device mount point.
         */
        std::filesystem::path getPathToPersistentMemoryDeviceMountPoint() const;
    };
};
