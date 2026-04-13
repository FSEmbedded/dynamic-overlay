#include "posix_utils.h"
#include "logging.h"

#include <cerrno>
#include <cstring>

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
}

bool posix_utils::path_exists(std::string_view path) noexcept
{
    struct stat st {};
    const std::string p(path);
    return ::stat(p.c_str(), &st) == 0;
}

bool posix_utils::is_directory(std::string_view path) noexcept
{
    struct stat st {};
    const std::string p(path);
    if (::stat(p.c_str(), &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

Error posix_utils::mkdir_p(std::string_view path, mode_t mode) noexcept
{
    std::string dir(path);

    for (std::size_t pos = 1; pos < dir.size(); ++pos) {
        if (dir[pos] == '/') {
            dir[pos] = '\0';
            if (::mkdir(dir.c_str(), mode) != 0 && errno != EEXIST) {
                LOG_ERRNO("mkdir_p failed", errno);
                return Error::directory_create_failed;
            }
            dir[pos] = '/';
        }
    }

    if (::mkdir(dir.c_str(), mode) != 0 && errno != EEXIST) {
        LOG_ERRNO("mkdir_p failed", errno);
        return Error::directory_create_failed;
    }

    return Error::none;
}

Error posix_utils::read_file_to_string(std::string_view path, std::string &out) noexcept
{
    const std::string p(path);
    FdGuard fd(::open(p.c_str(), O_RDONLY | O_CLOEXEC));
    if (!fd.valid()) {
        LOG_ERRNO("read_file_to_string: open failed", errno);
        return Error::open_failed;
    }

    struct stat st {};
    if (::fstat(fd.get(), &st) != 0) {
        LOG_ERRNO("read_file_to_string: fstat failed", errno);
        return Error::read_failed;
    }

    out.clear();
    if (st.st_size > 0) {
        out.resize(static_cast<std::size_t>(st.st_size));
        ssize_t total = 0;
        while (total < st.st_size) {
            const ssize_t n = ::read(fd.get(), &out[static_cast<std::size_t>(total)],
                                     static_cast<std::size_t>(st.st_size - total));
            if (n < 0) {
                if (errno == EINTR) continue;
                LOG_ERRNO("read_file_to_string: read failed", errno);
                return Error::read_failed;
            }
            if (n == 0) break;
            total += n;
        }
        out.resize(static_cast<std::size_t>(total));
    } else {
        // Size unknown (procfs, sysfs) — read in chunks
        char buf[4096];
        for (;;) {
            const ssize_t n = ::read(fd.get(), buf, sizeof(buf));
            if (n < 0) {
                if (errno == EINTR) continue;
                LOG_ERRNO("read_file_to_string: read failed", errno);
                return Error::read_failed;
            }
            if (n == 0) break;
            out.append(buf, static_cast<std::size_t>(n));
        }
    }

    return Error::none;
}

Error posix_utils::write_string_to_file(std::string_view path, std::string_view content,
                                         mode_t mode) noexcept
{
    const std::string p(path);
    FdGuard fd(::open(p.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode));
    if (!fd.valid()) {
        LOG_ERRNO("write_string_to_file: open failed", errno);
        return Error::open_failed;
    }

    std::size_t written = 0;
    while (written < content.size()) {
        const ssize_t n = ::write(fd.get(), content.data() + written,
                                   content.size() - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            LOG_ERRNO("write_string_to_file: write failed", errno);
            return Error::write_failed;
        }
        written += static_cast<std::size_t>(n);
    }

    return Error::none;
}

Error posix_utils::copy_file(std::string_view src, std::string_view dst) noexcept
{
    const std::string src_path(src);
    const std::string dst_path(dst);

    FdGuard fd_src(::open(src_path.c_str(), O_RDONLY | O_CLOEXEC));
    if (!fd_src.valid()) {
        LOG_ERRNO("copy_file: open source failed", errno);
        return Error::open_failed;
    }

    struct stat st {};
    if (::fstat(fd_src.get(), &st) != 0) {
        LOG_ERRNO("copy_file: fstat failed", errno);
        return Error::read_failed;
    }

    FdGuard fd_dst(::open(dst_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                          st.st_mode & 07777));
    if (!fd_dst.valid()) {
        LOG_ERRNO("copy_file: open dest failed", errno);
        return Error::open_failed;
    }

    char buf[4096];
    for (;;) {
        const ssize_t n = ::read(fd_src.get(), buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) continue;
            LOG_ERRNO("copy_file: read failed", errno);
            return Error::read_failed;
        }
        if (n == 0) break;

        std::size_t written = 0;
        while (written < static_cast<std::size_t>(n)) {
            const ssize_t w = ::write(fd_dst.get(), buf + written,
                                       static_cast<std::size_t>(n) - written);
            if (w < 0) {
                if (errno == EINTR) continue;
                LOG_ERRNO("copy_file: write failed", errno);
                return Error::write_failed;
            }
            written += static_cast<std::size_t>(w);
        }
    }

    return Error::none;
}

Error posix_utils::remove_file(std::string_view path) noexcept
{
    const std::string p(path);
    if (::unlink(p.c_str()) != 0 && errno != ENOENT) {
        LOG_ERRNO("remove_file failed", errno);
        return Error::remove_failed;
    }
    return Error::none;
}

Error posix_utils::rename_file(std::string_view old_path, std::string_view new_path) noexcept
{
    const std::string o(old_path);
    const std::string n(new_path);
    if (::rename(o.c_str(), n.c_str()) != 0) {
        LOG_ERRNO("rename_file failed", errno);
        return Error::rename_failed;
    }
    return Error::none;
}
