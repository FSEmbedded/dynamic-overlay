/**
 * Abstact libubootenv and offers a interface for reading UBoot-Environment.
 */

#pragma once
#include <cstdlib>
#include <cstdio>
#include <string>
#include <exception>
#include <cerrno>
#include <filesystem>
#include "persistent_mem_detector.h"

extern "C"
{
#include <libuboot.h>
}

class UBootEnvAccess : public std::exception
{
    private:
        std::string error_string;

    public:
        /**
             * Can not access UBoot environment.
             * @param var_name Variable that can not be read.
             */
        UBootEnvAccess(const std::string &var_name)
        {
            this->error_string = std::string("Error while access U-Boot Env; variable: \"") + var_name + std::string("\"");
        }
        const char *what() const throw()
        {
            return this->error_string.c_str();
        }
};

class UBootEnv : public std::exception
{
    private:
        std::string error_string;

    public:
        /**
         * libubootenv can not access UBoot Environment.
         * @param error_string Error description during execution.
         */
        UBootEnv(const std::string &error_string)
        {
            this->error_string = std::string("Error during access U-Boot Env: ") + error_string;
        }
        const char *what() const throw()
        {
            return this->error_string.c_str();
        }
};

class UBoot
{
    private:
        const std::filesystem::path fw_env_config_path;

    public:
        UBoot(const std::filesystem::path &);
        ~UBoot();

        UBoot(const UBoot &) = delete;
        UBoot &operator=(const UBoot &) = delete;
        UBoot(UBoot &&) = delete;
        UBoot &operator=(UBoot &&) = delete;

        /**
         * Return variable from UBoot-Environment.
         * @param variableName Variable that should be read from UBoot-Environment.
         * @throw UBootEnvAccess Error during reading variable from UBoot-Environment.
         * @throw UBootEnv Error during access UBoot-Environment.
         */
        const std::string getVariable(const std::string &) const;
};
