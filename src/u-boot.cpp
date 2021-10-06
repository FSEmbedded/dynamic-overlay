#include "u-boot.h"
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <algorithm>

UBoot::UBoot(const std::filesystem::path & path):
    fw_env_config_path(path)
{

}

UBoot::~UBoot()
{

}

std::string UBoot::getVariable(const std::string &variableName) const
{
    struct uboot_ctx *ctx;

    if (libuboot_initialize(&ctx, NULL) < 0)
    {
        throw(UBootEnv("Init libuboot failed"));
    }

    if (libuboot_read_config(ctx, this->fw_env_config_path.c_str()) < 0)
    {
        libuboot_exit(ctx);
        throw(UBootEnv(std::string("Reading ") + this->fw_env_config_path.string() + std::string(" failed")));
    }

    if (libuboot_open(ctx) < 0)
    {
        libuboot_exit(ctx);
        throw(UBootEnv("Opening of ENV failed"));
    }

    const char *ptr_var = libuboot_get_env(ctx, variableName.c_str());
    if (ptr_var == NULL)
    {
        libuboot_close(ctx);
        libuboot_exit(ctx);
        throw(UBootEnvAccess(variableName));
    }

    std::string returnValue(ptr_var);
    free((void *)ptr_var);

    libuboot_close(ctx);
    libuboot_exit(ctx);
    return returnValue;
}

uint8_t UBoot::getVariable(const std::string &variable_name, const std::vector<uint8_t> &allowed_list)
{
    const std::string content = this->getVariable(variable_name);
    unsigned long number;
    try
    {
        number = std::stoul(content);
    }
    catch(...)
    {
        throw(UBootEnvVarCanNotConvertedIntoReturnType("Variable content can not be converted into a unsigned long"));
    }

    uint8_t return_value;
    if(number <= UCHAR_MAX)
    {
        return_value = uint8_t(number);
    }
    else
    {
        throw(UBootEnvVarCanNotConvertedIntoReturnType("Variable fit not in type u_int8"));
    }

    if(std::find(allowed_list.begin(), allowed_list.end(), return_value) == allowed_list.end())
    {
        std::string allowed_list_ser;
        for(const auto & elem : allowed_list)
        {
            allowed_list_ser += std::to_string(elem) + std::string(" ");
        }

        throw(UBootEnvVarNotAllowedContent(std::to_string(return_value), allowed_list_ser));
    }

    return return_value;
}

std::string UBoot::getVariable(const std::string &variable_name, const std::vector<std::string> &allowed_list)
{
    const std::string return_value = this->getVariable(variable_name);
    if(std::find(allowed_list.begin(), allowed_list.end(), return_value) == allowed_list.end())
    {
        std::string allowed_list_ser;
        for(const auto & elem : allowed_list)
        {
            allowed_list_ser += elem + std::string(" ");
        }

        throw(UBootEnvVarNotAllowedContent(return_value, allowed_list_ser));
    }
    return return_value;
}

char UBoot::getVariable(const std::string &variable_name, const std::vector<char> &allowed_list)
{
    const std::string content = this->getVariable(variable_name);

    if(content.size() != 1)
    {
        throw(UBootEnvVarCanNotConvertedIntoReturnType("Variable fit not in type char"));
    }

    const char return_value = content.at(0);

    if(std::find(allowed_list.begin(), allowed_list.end(), return_value) == allowed_list.end())
    {
        std::string allowed_list_ser;
        for(const auto & elem : allowed_list)
        {
            allowed_list_ser += elem + std::string(" ");
        }

        throw(UBootEnvVarNotAllowedContent(std::to_string(return_value), allowed_list_ser));
    }

    return return_value;
}
