#pragma once

#include <regex>
#include <exception>
#include <string>

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
            explicit ErrorOpenKernelParam(const std::string & msg)
            {
                this->error_msg = std::string("Could not open kernel commandline parameters: ") + msg;
            }
            const char * what() const throw () {
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
            const char * what() const throw () {
                return this->error_msg.c_str();
            }      
    };
    
    //////////////////////////////////////////////////////////////////////////////

    class PersistentMemDetector
    {
        private:
            std::regex nand_memory, emmc_memory;

        public:
            /**
             * Set regex detection for NAND and eMMC.
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
             * @throw ErrorOpenKernelParam
             * @throw ErrorDeterminePersistentMemory
             */
            MemType getMemType();
    };
};