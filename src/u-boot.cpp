#include "u-boot.h"

UBoot::UBoot()
{

}

UBoot::~UBoot()
{

}

const std::string UBoot::getVariable(const std::string & variableName) const
{
    struct uboot_ctx *ctx;

    if (libuboot_initialize(&ctx, NULL) < 0)
    {
        throw(UBootEnv("Init libuboot failed"));
    }

    if (libuboot_read_config(ctx, "/etc/fw_env.config") < 0)
    {
	    libuboot_exit(ctx);
        throw(UBootEnv("Reading fw_env.config failed"));
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
    free( (void*) ptr_var);
	libuboot_exit(ctx);
    return returnValue;
}