#include <gtest/gtest.h>
#include "posix_utils.h"

namespace {

TEST(ScopeGuard, RunsOnDestruction) {
    bool called = false;
    {
        ScopeGuard guard([&called]() { called = true; });
        EXPECT_FALSE(called);
    }
    EXPECT_TRUE(called);
}

TEST(ScopeGuard, DismissPreventsExecution) {
    bool called = false;
    {
        ScopeGuard guard([&called]() { called = true; });
        guard.dismiss();
    }
    EXPECT_FALSE(called);
}

TEST(ScopeGuard, MultipleDismissIsSafe) {
    bool called = false;
    {
        ScopeGuard guard([&called]() { called = true; });
        guard.dismiss();
        guard.dismiss();
    }
    EXPECT_FALSE(called);
}

TEST(ScopeGuard, RunsOnEarlyReturn) {
    bool called = false;
    auto run = [&called]() {
        ScopeGuard guard([&called]() { called = true; });
        if (true) return; // simulate early exit
    };
    run();
    EXPECT_TRUE(called);
}

TEST(ScopeGuard, CleanupWithFileRemoval) {
    // Verify ScopeGuard works with real POSIX cleanup (mirrors cert store usage)
    const std::string path = "/tmp/scope_guard_test_file";
    ASSERT_EQ(posix_utils::write_string_to_file(path, "test"), Error::none);
    EXPECT_TRUE(posix_utils::path_exists(path));

    {
        ScopeGuard guard([&path]() {
            static_cast<void>(posix_utils::remove_file(path));
        });
    }
    EXPECT_FALSE(posix_utils::path_exists(path));
}

TEST(ScopeGuard, DismissKeepsFile) {
    const std::string path = "/tmp/scope_guard_dismiss_test";
    ASSERT_EQ(posix_utils::write_string_to_file(path, "keep"), Error::none);

    {
        ScopeGuard guard([&path]() {
            static_cast<void>(posix_utils::remove_file(path));
        });
        guard.dismiss();
    }
    EXPECT_TRUE(posix_utils::path_exists(path));

    // Cleanup
    static_cast<void>(posix_utils::remove_file(path));
}

} // namespace
