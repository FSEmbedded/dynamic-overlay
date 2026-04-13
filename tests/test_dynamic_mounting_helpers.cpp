/**
 * Unit tests for DynamicMounting helper logic and INI parser
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "error.h"
#include "ini_parser.h"

namespace {

// --- has_identical_paths logic ---

static bool has_identical_paths(std::string_view lower_dir)
{
    std::vector<std::string> paths;
    while (!lower_dir.empty()) {
        const auto pos = lower_dir.find(':');
        auto token = lower_dir.substr(0, pos);
        if (!token.empty()) {
            paths.emplace_back(token);
        }
        if (pos == std::string_view::npos) {
            break;
        }
        lower_dir.remove_prefix(pos + 1);
    }
    std::sort(paths.begin(), paths.end());
    return std::adjacent_find(paths.begin(), paths.end()) != paths.end();
}

class HasIdenticalPathsTest : public ::testing::Test {};

TEST_F(HasIdenticalPathsTest, NoDuplicates) {
    EXPECT_FALSE(has_identical_paths("/etc:/usr:/var"));
    EXPECT_FALSE(has_identical_paths("/a:/b"));
    EXPECT_FALSE(has_identical_paths("/single"));
}

TEST_F(HasIdenticalPathsTest, WithDuplicates) {
    EXPECT_TRUE(has_identical_paths("/etc:/etc"));
    EXPECT_TRUE(has_identical_paths("/a:/b:/a"));
    EXPECT_TRUE(has_identical_paths("/path:/other:/path"));
}

TEST_F(HasIdenticalPathsTest, EmptyPath) {
    EXPECT_FALSE(has_identical_paths(""));
}

TEST_F(HasIdenticalPathsTest, AdjacentDuplicates) {
    EXPECT_TRUE(has_identical_paths("/a:/a:/b"));
    EXPECT_TRUE(has_identical_paths("/a:/b:/b"));
}

// --- Section name matching ---

class SectionMatchingTest : public ::testing::Test {};

TEST_F(SectionMatchingTest, ApplicationFolderSection) {
    auto is_application_folder = [](std::string_view name) {
        return name == "ApplicationFolder";
    };

    EXPECT_TRUE(is_application_folder("ApplicationFolder"));
    EXPECT_FALSE(is_application_folder("applicationfolder"));
    EXPECT_FALSE(is_application_folder("ApplicationFolder.test"));
    EXPECT_FALSE(is_application_folder("PersistentMemory.etc"));
}

TEST_F(SectionMatchingTest, PersistentMemorySection) {
    auto is_persistent_memory = [](std::string_view name) {
        constexpr std::string_view prefix = "PersistentMemory.";
        return name.size() > prefix.size() &&
               name.substr(0, prefix.size()) == prefix;
    };

    EXPECT_TRUE(is_persistent_memory("PersistentMemory.etc"));
    EXPECT_TRUE(is_persistent_memory("PersistentMemory.usr"));
    EXPECT_TRUE(is_persistent_memory("PersistentMemory.some.nested"));
    EXPECT_FALSE(is_persistent_memory("PersistentMemory."));
    EXPECT_FALSE(is_persistent_memory("PersistentMemory"));
    EXPECT_FALSE(is_persistent_memory("ApplicationFolder"));
}

// --- INI parser ---

class IniParserTest : public ::testing::Test {};

TEST_F(IniParserTest, ParseSimpleConfig) {
    constexpr std::string_view input =
        "[ApplicationFolder]\n"
        "lower=/app\n"
        "merge=/merged\n"
        "\n"
        "[PersistentMemory.etc]\n"
        "lower=/etc\n"
        "upper=/upper/etc\n"
        "work=/work/etc\n"
        "merge=/etc\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::none);
    EXPECT_EQ(cfg.size(), 2u);
    EXPECT_EQ(cfg.at("ApplicationFolder").at("lower"), "/app");
    EXPECT_EQ(cfg.at("PersistentMemory.etc").at("merge"), "/etc");
}

TEST_F(IniParserTest, SkipCommentsAndEmptyLines) {
    constexpr std::string_view input =
        "# comment\n"
        "; another comment\n"
        "\n"
        "[Section]\n"
        "key=value\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::none);
    EXPECT_EQ(cfg.size(), 1u);
    EXPECT_EQ(cfg.at("Section").at("key"), "value");
}

TEST_F(IniParserTest, TrimWhitespace) {
    constexpr std::string_view input =
        "[Section]\n"
        "  key  =  value  \n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::none);
    EXPECT_EQ(cfg.at("Section").at("key"), "value");
}

TEST_F(IniParserTest, EmptyInput) {
    ini::Config cfg;
    EXPECT_EQ(ini::parse_string("", cfg), Error::none);
    EXPECT_TRUE(cfg.empty());
}

// --- Error enum ---

class ErrorTest : public ::testing::Test {};

TEST_F(ErrorTest, NoneIsZero) {
    EXPECT_EQ(static_cast<uint8_t>(Error::none), 0u);
}

TEST_F(ErrorTest, ErrorToString) {
    EXPECT_EQ(error_to_string(Error::none), "no error");
    EXPECT_EQ(error_to_string(Error::mount_failed), "mount failed");
    EXPECT_EQ(error_to_string(Error::config_invalid), "configuration invalid");
}

TEST_F(ErrorTest, AllCodesHaveStrings) {
    // Verify no code returns "unknown error"
    constexpr std::string_view unknown = "unknown error";
    EXPECT_NE(error_to_string(Error::none), unknown);
    EXPECT_NE(error_to_string(Error::mount_failed), unknown);
    EXPECT_NE(error_to_string(Error::uboot_init_failed), unknown);
    EXPECT_NE(error_to_string(Error::cert_store_failed), unknown);
    EXPECT_NE(error_to_string(Error::json_parse_failed), unknown);
    EXPECT_NE(error_to_string(Error::remove_failed), unknown);
}

} // namespace
