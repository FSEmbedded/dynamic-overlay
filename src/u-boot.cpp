#include "u-boot.h"
#include "logging.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <cstring>

UBoot::UBoot(std::string_view path) noexcept
    : fw_env_config_path_(path)
{
}

Error UBoot::get_raw_variable(std::string_view name, std::string &out) const noexcept
{
    UBootCtxGuard ctx;

    Error err = ctx.init();
    if (err != Error::none) {
        LOG_ERROR("libuboot init failed");
        return err;
    }

    err = ctx.read_config(fw_env_config_path_);
    if (err != Error::none) {
        LOG_ERROR("reading " + fw_env_config_path_ + " failed");
        return err;
    }

    err = ctx.open_env();
    if (err != Error::none) {
        LOG_ERROR("opening U-Boot env failed");
        return err;
    }

    const std::string var_name(name);
    const char *ptr_var = libuboot_get_env(ctx.get(), var_name.c_str());
    if (ptr_var == nullptr) {
        LOG_ERROR("U-Boot variable not found: " + var_name);
        return Error::uboot_var_not_found;
    }

    out = std::string(ptr_var);
    std::free(const_cast<char *>(ptr_var));

    return Error::none;
}

Error UBoot::getVariable(std::string_view name, std::string &out) const noexcept
{
    return get_raw_variable(name, out);
}

Error UBoot::getVariable(std::string_view name,
                          const std::vector<uint8_t> &allowed,
                          uint8_t &out) const noexcept
{
    std::string content;
    const Error err = get_raw_variable(name, content);
    if (err != Error::none) {
        return err;
    }

    unsigned long number = 0;
    const auto *begin = content.data();
    const auto *end = content.data() + content.size();
    const auto [ptr, ec] = std::from_chars(begin, end, number);

    if (ec != std::errc{} || ptr != end) {
        LOG_ERROR("variable cannot be converted to uint8: " + content);
        return Error::uboot_var_invalid;
    }

    if (number > 255) {
        LOG_ERROR("variable value out of uint8 range: " + content);
        return Error::uboot_var_invalid;
    }

    out = static_cast<uint8_t>(number);

    if (std::find(allowed.begin(), allowed.end(), out) == allowed.end()) {
        LOG_ERROR("variable value not in allowed list: " + content);
        return Error::uboot_var_invalid;
    }

    return Error::none;
}

Error UBoot::getVariable(std::string_view name,
                          const std::vector<std::string> &allowed,
                          std::string &out) const noexcept
{
    const Error err = get_raw_variable(name, out);
    if (err != Error::none) {
        return err;
    }

    if (std::find(allowed.begin(), allowed.end(), out) == allowed.end()) {
        LOG_ERROR("variable value not in allowed list: " + out);
        return Error::uboot_var_invalid;
    }

    return Error::none;
}

Error UBoot::getVariable(std::string_view name,
                          const std::vector<char> &allowed,
                          char &out) const noexcept
{
    std::string content;
    const Error err = get_raw_variable(name, content);
    if (err != Error::none) {
        return err;
    }

    if (content.size() != 1) {
        LOG_ERROR("variable cannot be converted to char: " + content);
        return Error::uboot_var_invalid;
    }

    out = content[0];

    if (std::find(allowed.begin(), allowed.end(), out) == allowed.end()) {
        LOG_ERROR("variable value not in allowed list: " + content);
        return Error::uboot_var_invalid;
    }

    return Error::none;
}
