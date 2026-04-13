#pragma once

#include "error.h"

#include <cstddef>
#include <string>
#include <string_view>

extern "C" {
#include <unistd.h>
}

// RAII wrapper for file descriptors — closes on destruction
class FdGuard {
    int fd_{-1};

public:
    FdGuard() noexcept = default;
    explicit FdGuard(int fd) noexcept : fd_{fd} {}

    ~FdGuard() noexcept
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    FdGuard(const FdGuard &) = delete;
    FdGuard &operator=(const FdGuard &) = delete;

    FdGuard(FdGuard &&other) noexcept : fd_{other.fd_}
    {
        other.fd_ = -1;
    }

    FdGuard &operator=(FdGuard &&other) noexcept
    {
        if (this != &other) {
            if (fd_ >= 0) {
                ::close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }

    int release() noexcept
    {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

    void reset(int fd = -1) noexcept
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = fd;
    }
};

// RAII scope guard — runs cleanup action on destruction unless dismissed
template <typename Func>
class ScopeGuard {
    Func fn_;
    bool active_{true};

public:
    explicit ScopeGuard(Func fn) noexcept : fn_(fn) {}
    ~ScopeGuard() noexcept { if (active_) { fn_(); } }

    void dismiss() noexcept { active_ = false; }

    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;
    ScopeGuard(ScopeGuard &&) = delete;
    ScopeGuard &operator=(ScopeGuard &&) = delete;
};

namespace posix_utils {

[[nodiscard]] bool path_exists(std::string_view path) noexcept;

[[nodiscard]] bool is_directory(std::string_view path) noexcept;

[[nodiscard]] Error mkdir_p(std::string_view path, mode_t mode = 0755) noexcept;

[[nodiscard]] Error read_file_to_string(std::string_view path, std::string &out) noexcept;

[[nodiscard]] Error write_string_to_file(std::string_view path, std::string_view content,
                                          mode_t mode = 0644) noexcept;

[[nodiscard]] Error copy_file(std::string_view src, std::string_view dst) noexcept;

[[nodiscard]] Error remove_file(std::string_view path) noexcept;

[[nodiscard]] Error rename_file(std::string_view old_path, std::string_view new_path) noexcept;

} // namespace posix_utils
