#pragma once

#include <regex>
#include <exception>
#include <string>

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
            explicit ErrorOpenKernelParam(const std::string & msg)
            {
                this->error_msg = std::string("Could not open kernel comandline parameters: ") + msg;
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
            PersistentMemDetector();
            ~PersistentMemDetector();

            PersistentMemDetector(const PersistentMemDetector &) = delete;
            PersistentMemDetector &operator=(const PersistentMemDetector &) = delete;
            PersistentMemDetector(PersistentMemDetector &&) = delete;
            PersistentMemDetector &operator=(PersistentMemDetector &&) = delete;

            MemType getMemType();
    };
};