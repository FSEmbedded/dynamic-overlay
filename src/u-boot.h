#pragma once
#include <cstdlib>
#include <cstdio>
#include <string>
#include <exception>
#include <cerrno>

extern "C" {
    #include <libuboot.h>
}

class UBootEnvAccess : public std::exception
{
    private:
        std::string error_string;
    public:
        UBootEnvAccess(const std::string &var_name)
        {
            this->error_string = std::string("Error while access U-Boot Env; variable: \"")
                                 + var_name + std::string("\"");

        }
        const char * what() const throw () {
            return this->error_string.c_str();
        }
};

class UBootEnv : public std::exception
{
    private:
        std::string error_string;
    public:
        UBootEnv(const std::string &error_string)
        {
            this->error_string = std::string("Error during access U-Boot Env: ") + error_string;
        }
        const char * what() const throw () {
            return this->error_string.c_str();
        }
};

class UBoot
{
    public:
        UBoot();
        UBoot(const UBoot &) = delete;
        UBoot &operator=(const UBoot &) = delete;
        UBoot(UBoot &&) = delete;
        UBoot &operator=(UBoot &&) = delete;
            
        ~UBoot();
        const std::string getVariable(const std::string &) const;

};