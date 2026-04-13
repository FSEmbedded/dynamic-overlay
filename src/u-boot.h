#pragma once

#include "error.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
    #include <libuboot.h>
}

// RAII guard for libuboot context — init → read_config → open_env → get_env
class UBootCtxGuard {
    struct uboot_ctx *ctx_ = nullptr;
    bool opened_ = false;

public:
    UBootCtxGuard() noexcept = default;

    ~UBootCtxGuard() noexcept
    {
        if (opened_) {
            libuboot_close(ctx_);
        }
        if (ctx_) {
            libuboot_exit(ctx_);
        }
    }

    UBootCtxGuard(const UBootCtxGuard &) = delete;
    UBootCtxGuard &operator=(const UBootCtxGuard &) = delete;
    UBootCtxGuard(UBootCtxGuard &&) = delete;
    UBootCtxGuard &operator=(UBootCtxGuard &&) = delete;

    [[nodiscard]] Error init() noexcept
    {
        if (libuboot_initialize(&ctx_, nullptr) < 0) {
            return Error::uboot_init_failed;
        }
        return Error::none;
    }

    [[nodiscard]] Error read_config(std::string_view path) noexcept
    {
        const std::string p(path);
        if (libuboot_read_config(ctx_, p.c_str()) < 0) {
            return Error::uboot_read_failed;
        }
        return Error::none;
    }

    [[nodiscard]] Error open_env() noexcept
    {
        if (libuboot_open(ctx_) < 0) {
            return Error::uboot_read_failed;
        }
        opened_ = true;
        return Error::none;
    }

    [[nodiscard]] struct uboot_ctx *get() const noexcept { return ctx_; }
};

class UBoot
{
    const std::string fw_env_config_path_;

    [[nodiscard]] Error get_raw_variable(std::string_view name, std::string &out) const noexcept;

public:
    explicit UBoot(std::string_view path) noexcept;
    ~UBoot() noexcept = default;

    UBoot(const UBoot &) = delete;
    UBoot &operator=(const UBoot &) = delete;
    UBoot(UBoot &&) = delete;
    UBoot &operator=(UBoot &&) = delete;

    [[nodiscard]] Error getVariable(std::string_view name, std::string &out) const noexcept;

    [[nodiscard]] Error getVariable(std::string_view name,
                                     const std::vector<uint8_t> &allowed,
                                     uint8_t &out) const noexcept;

    [[nodiscard]] Error getVariable(std::string_view name,
                                     const std::vector<std::string> &allowed,
                                     std::string &out) const noexcept;

    [[nodiscard]] Error getVariable(std::string_view name,
                                     const std::vector<char> &allowed,
                                     char &out) const noexcept;
};
