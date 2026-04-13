#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

extern "C" {
#include <fcntl.h>
#include <unistd.h>
}

namespace logging {

enum class Level : uint8_t {
    fatal,
    error,
    warning,
    info,
    debug
};

inline constexpr std::string_view level_tag(Level lvl) noexcept
{
    switch (lvl) {
    case Level::fatal:   return "FATAL: ";
    case Level::error:   return "ERROR: ";
    case Level::warning: return "WARNING: ";
    case Level::info:    return "INFO: ";
    case Level::debug:   return "DEBUG: ";
    }
    return "";
}

inline constexpr std::string_view prefix() noexcept
{
    return "dynamicoverlay: ";
}

#ifdef ENABLE_LOGGING

inline void log_write(Level lvl, std::string_view msg) noexcept
{
    std::string buf;
    buf.reserve(prefix().size() + level_tag(lvl).size() + msg.size() + 1);
    buf.append(prefix());
    buf.append(level_tag(lvl));
    buf.append(msg);
    buf.push_back('\n');

#ifdef LOG_BACKEND_KMSG
    // /dev/kmsg backend for preinit (before syslog/journald)
    const int fd = ::open("/dev/kmsg", O_WRONLY | O_NOCTTY);
    if (fd >= 0) {
        if (::write(fd, buf.data(), buf.size()) == -1) { /* ignore */ }
        ::close(fd);
    } else {
        // Fallback to stderr if kmsg unavailable
        if (::write(STDERR_FILENO, buf.data(), buf.size()) == -1) { /* ignore */ }
    }
#else
    if (::write(STDERR_FILENO, buf.data(), buf.size()) == -1) { /* ignore */ }
#endif
}

inline void log_write_errno(Level lvl, std::string_view msg, int errnum) noexcept
{
    const char *errstr = std::strerror(errnum);
    std::string full;
    full.reserve(msg.size() + 2 + std::strlen(errstr));
    full.append(msg);
    full.append(": ");
    full.append(errstr);
    log_write(lvl, full);
}

#define LOG_FATAL(msg) ::logging::log_write(::logging::Level::fatal, msg)
#define LOG_ERROR(msg) ::logging::log_write(::logging::Level::error, msg)
#define LOG_WARNING(msg) ::logging::log_write(::logging::Level::warning, msg)
#define LOG_INFO(msg) ::logging::log_write(::logging::Level::info, msg)

#define LOG_ERRNO(msg, errnum) ::logging::log_write_errno(::logging::Level::error, msg, errnum)

#ifdef ENABLE_DEBUG_LOGGING
    #define LOG_DEBUG(msg) ::logging::log_write(::logging::Level::debug, msg)
#else
    #define LOG_DEBUG(msg) ((void)0)
#endif

#else
    // Logging disabled — all macros compile out to nothing
    #define LOG_FATAL(msg)        ((void)0)
    #define LOG_ERROR(msg)        ((void)0)
    #define LOG_WARNING(msg)      ((void)0)
    #define LOG_INFO(msg)         ((void)0)
    #define LOG_DEBUG(msg)        ((void)0)
    #define LOG_ERRNO(msg, errnum) ((void)0)
#endif

} // namespace logging
