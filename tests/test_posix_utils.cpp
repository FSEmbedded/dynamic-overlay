#include <gtest/gtest.h>
#include "posix_utils.h"

#include <cstdlib>
#include <string>

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
}

namespace {

class TempDir {
    std::string path_;

public:
    TempDir()
    {
        char tmpl[] = "/tmp/dynoverlay_test_XXXXXX";
        char *dir = ::mkdtemp(tmpl);
        if (dir) path_ = dir;
    }

    ~TempDir()
    {
        if (!path_.empty()) {
            // Clean up recursively
            const std::string cmd = "rm -rf " + path_;
            (void)std::system(cmd.c_str());
        }
    }

    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;

    [[nodiscard]] const std::string &path() const { return path_; }
    [[nodiscard]] bool valid() const { return !path_.empty(); }
};

// --- FdGuard ---

TEST(FdGuard, DefaultConstructorInvalid) {
    FdGuard guard;
    EXPECT_FALSE(guard.valid());
    EXPECT_EQ(guard.get(), -1);
}

TEST(FdGuard, ValidFd) {
    // pipe() gives us two valid fds
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    {
        FdGuard read_guard(fds[0]);
        FdGuard write_guard(fds[1]);
        EXPECT_TRUE(read_guard.valid());
        EXPECT_TRUE(write_guard.valid());
    }
    // After destruction, fds should be closed
    // Writing to closed fd should fail
    EXPECT_EQ(::write(fds[1], "x", 1), -1);
}

TEST(FdGuard, MoveConstructor) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    ::close(fds[1]);

    FdGuard original(fds[0]);
    FdGuard moved(std::move(original));

    EXPECT_FALSE(original.valid());
    EXPECT_TRUE(moved.valid());
    EXPECT_EQ(moved.get(), fds[0]);
}

TEST(FdGuard, MoveAssignment) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);

    FdGuard a(fds[0]);
    FdGuard b(fds[1]);

    b = std::move(a);

    EXPECT_FALSE(a.valid());
    EXPECT_TRUE(b.valid());
    EXPECT_EQ(b.get(), fds[0]);
    // fds[1] should have been closed by the assignment
    EXPECT_EQ(::write(fds[1], "x", 1), -1);
}

TEST(FdGuard, Release) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    ::close(fds[1]);

    int raw_fd;
    {
        FdGuard guard(fds[0]);
        raw_fd = guard.release();
        EXPECT_FALSE(guard.valid());
    }
    // fd should still be open after guard destruction — verify via fcntl
    EXPECT_NE(::fcntl(raw_fd, F_GETFD), -1);
    ::close(raw_fd);
}

TEST(FdGuard, Reset) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);

    FdGuard guard(fds[0]);
    guard.reset(fds[1]);

    EXPECT_EQ(guard.get(), fds[1]);
    // fds[0] should have been closed by reset
    EXPECT_EQ(::write(fds[0], "x", 1), -1);
}

// --- path_exists ---

TEST(PathExists, ExistingPath) {
    EXPECT_TRUE(posix_utils::path_exists("/tmp"));
}

TEST(PathExists, NonExistingPath) {
    EXPECT_FALSE(posix_utils::path_exists("/nonexistent_path_12345"));
}

// --- is_directory ---

TEST(IsDirectory, Directory) {
    EXPECT_TRUE(posix_utils::is_directory("/tmp"));
}

TEST(IsDirectory, NonExistingPath) {
    EXPECT_FALSE(posix_utils::is_directory("/nonexistent_12345"));
}

// --- mkdir_p ---

TEST(MkdirP, CreateSingleLevel) {
    TempDir tmp;
    ASSERT_TRUE(tmp.valid());

    const std::string dir = tmp.path() + "/newdir";
    EXPECT_EQ(posix_utils::mkdir_p(dir), Error::none);
    EXPECT_TRUE(posix_utils::is_directory(dir));
}

TEST(MkdirP, CreateNestedLevels) {
    TempDir tmp;
    ASSERT_TRUE(tmp.valid());

    const std::string dir = tmp.path() + "/a/b/c";
    EXPECT_EQ(posix_utils::mkdir_p(dir), Error::none);
    EXPECT_TRUE(posix_utils::is_directory(dir));
}

TEST(MkdirP, ExistingDirectory) {
    EXPECT_EQ(posix_utils::mkdir_p("/tmp"), Error::none);
}

// --- read/write file ---

TEST(ReadWriteFile, RoundTrip) {
    TempDir tmp;
    ASSERT_TRUE(tmp.valid());

    const std::string file = tmp.path() + "/test.txt";
    constexpr std::string_view content = "hello world\nline two\n";

    EXPECT_EQ(posix_utils::write_string_to_file(file, content), Error::none);

    std::string readback;
    EXPECT_EQ(posix_utils::read_file_to_string(file, readback), Error::none);
    EXPECT_EQ(readback, content);
}

TEST(ReadWriteFile, EmptyFile) {
    TempDir tmp;
    ASSERT_TRUE(tmp.valid());

    const std::string file = tmp.path() + "/empty.txt";
    EXPECT_EQ(posix_utils::write_string_to_file(file, ""), Error::none);

    std::string readback;
    EXPECT_EQ(posix_utils::read_file_to_string(file, readback), Error::none);
    EXPECT_TRUE(readback.empty());
}

TEST(ReadWriteFile, NonExistentFileRead) {
    std::string out;
    EXPECT_NE(posix_utils::read_file_to_string("/nonexistent_12345", out), Error::none);
}

// --- copy_file ---

TEST(CopyFile, BasicCopy) {
    TempDir tmp;
    ASSERT_TRUE(tmp.valid());

    const std::string src = tmp.path() + "/src.txt";
    const std::string dst = tmp.path() + "/dst.txt";
    constexpr std::string_view content = "copy test data";

    EXPECT_EQ(posix_utils::write_string_to_file(src, content), Error::none);
    EXPECT_EQ(posix_utils::copy_file(src, dst), Error::none);

    std::string readback;
    EXPECT_EQ(posix_utils::read_file_to_string(dst, readback), Error::none);
    EXPECT_EQ(readback, content);
}

// --- remove_file ---

TEST(RemoveFile, ExistingFile) {
    TempDir tmp;
    ASSERT_TRUE(tmp.valid());

    const std::string file = tmp.path() + "/todelete.txt";
    EXPECT_EQ(posix_utils::write_string_to_file(file, "data"), Error::none);
    EXPECT_TRUE(posix_utils::path_exists(file));

    EXPECT_EQ(posix_utils::remove_file(file), Error::none);
    EXPECT_FALSE(posix_utils::path_exists(file));
}

TEST(RemoveFile, NonExistentFileTolerant) {
    EXPECT_EQ(posix_utils::remove_file("/tmp/nonexistent_12345"), Error::none);
}

// --- rename_file ---

TEST(RenameFile, BasicRename) {
    TempDir tmp;
    ASSERT_TRUE(tmp.valid());

    const std::string old_path = tmp.path() + "/old.txt";
    const std::string new_path = tmp.path() + "/new.txt";
    EXPECT_EQ(posix_utils::write_string_to_file(old_path, "data"), Error::none);

    EXPECT_EQ(posix_utils::rename_file(old_path, new_path), Error::none);
    EXPECT_FALSE(posix_utils::path_exists(old_path));
    EXPECT_TRUE(posix_utils::path_exists(new_path));
}

} // namespace
