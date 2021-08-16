#include "u-boot.h"

UBoot::UBoot(const std::filesystem::path & path):
    fw_env_config_path(path)
{

}

UBoot::~UBoot()
{

}

const std::string UBoot::getVariable(const std::string &variableName) const
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
        libuboot_exit(ctx);
        throw(UBootEnvAccess(variableName));
    }

    std::string returnValue(ptr_var);
    free((void *)ptr_var);
    libuboot_exit(ctx);
    return returnValue;
}
