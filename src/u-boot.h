/**
 * Abstact libubootenv and offers a interface for reading UBoot-Environment.
 */

#pragma once
#include <cstdlib>
#include <cstdio>
#include <string>
#include <exception>
#include <vector>

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
        explicit UBootEnvAccess(const std::string &var_name)
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
        explicit UBootEnv(const std::string &error_string)
        {
            this->error_string = std::string("Error during access U-Boot Env: ") + error_string;
        }
        const char *what() const throw()
        {
            return this->error_string.c_str();
        }
};

class UBootEnvVarNotAllowedContent : public std::exception
{
    private:
        std::string error_string;

    public:
        /**
         * Variable missmatched with allowed states inside UBoot variable.
         * @param content Actual content of uboot environment.
         * @param allowed List of allowed states as string.
         */
        explicit UBootEnvVarNotAllowedContent(const std::string &content, const std::string &allowed)
        {
            this->error_string = std::string("Variable does not allowed content: \"") + allowed + std::string("\" instead:");
            this->error_string += content + std::string("\"");
        }
        const char *what() const throw()
        {
            return this->error_string.c_str();
        }
};

class UBootEnvVarCanNotConvertedIntoReturnType : public std::exception
{
    private:
        std::string error_string;

    public:
        /**
         * UBoot-Enviornment variable can not be converted into given return value type.
         * @param content Variable content that occurs the error.
         */
        explicit UBootEnvVarCanNotConvertedIntoReturnType(const std::string &content)
        {
            this->error_string = std::string("Variable can not converted into return type: ") + content;
        }
        const char *what() const throw()
        {
            return this->error_string.c_str();
        }
};

class UBoot
{
    private:
        const std::string fw_env_config_path;
        /**
         * Return variable from UBoot-Environment.
         * @param variableName Variable that should be read from UBoot-Environment.
         * @return Content of variable.
         * @throw UBootEnvAccess Error during reading variable from UBoot-Environment.
         * @throw UBootEnv Error during access UBoot-Environment.
         */
        std::string getVariable(const std::string &) const;
    public:
        UBoot(const std::string &);
        ~UBoot();

        UBoot(const UBoot &) = delete;
        UBoot &operator=(const UBoot &) = delete;
        UBoot(UBoot &&) = delete;
        UBoot &operator=(UBoot &&) = delete;

        /**
         * Return variable from UBoot-Environment. Must match to type and given allowed list of content.
         * @param variableName Variable that should be read from UBoot-Environment.
         * @param allowed_list of allowed states inside the uboot variable.
         * @return Variable content in uint8 container.
         * @throw UBootEnvVarCanNotConvertedIntoReturnType
         * @throw UBootEnvVarNotAllowedContent
         */
        uint8_t getVariable(const std::string &, const std::vector<uint8_t> &);
        /**
         * Return variable from UBoot-Environment. Must match to type and given allowed list of content.
         * @param variableName Variable that should be read from UBoot-Environment.
         * @param allowed_list of allowed states inside the uboot variable.
         * @return Variable content in string container.
         * @throw UBootEnvVarCanNotConvertedIntoReturnType
         * @throw UBootEnvVarNotAllowedContent
         */
        std::string getVariable(const std::string &, const std::vector<std::string> &);
        /**
         * Return variable from UBoot-Environment. Must match to type and given allowed list of content.
         * @param variableName Variable that should be read from UBoot-Environment.
         * @param allowed_list of allowed states inside the uboot variable.
         * @return Variable content in char container.
         * @throw UBootEnvVarCanNotConvertedIntoReturnType
         * @throw UBootEnvVarNotAllowedContent
         */
        char getVariable(const std::string &, const std::vector<char> &);
};
