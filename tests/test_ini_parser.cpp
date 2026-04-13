#include <gtest/gtest.h>
#include "ini_parser.h"
#include "config.h"

namespace {

TEST(IniParser, EmptyInput) {
    ini::Config cfg;
    EXPECT_EQ(ini::parse_string("", cfg), Error::none);
    EXPECT_TRUE(cfg.empty());
}

TEST(IniParser, SingleSection) {
    constexpr std::string_view input =
        "[ApplicationFolder]\n"
        "lower=/app\n"
        "merge=/merged\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::none);
    ASSERT_EQ(cfg.size(), 1u);
    EXPECT_EQ(cfg.at("ApplicationFolder").at("lower"), "/app");
    EXPECT_EQ(cfg.at("ApplicationFolder").at("merge"), "/merged");
}

TEST(IniParser, MultipleSections) {
    constexpr std::string_view input =
        "[ApplicationFolder]\n"
        "lower=/app\n"
        "\n"
        "[PersistentMemory.etc]\n"
        "lowerdir=/etc\n"
        "upperdir=/upper/etc\n"
        "workdir=/work/etc\n"
        "merge=/etc\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::none);
    ASSERT_EQ(cfg.size(), 2u);
    EXPECT_EQ(cfg.at("ApplicationFolder").at("lower"), "/app");
    EXPECT_EQ(cfg.at("PersistentMemory.etc").at("lowerdir"), "/etc");
    EXPECT_EQ(cfg.at("PersistentMemory.etc").at("upperdir"), "/upper/etc");
}

TEST(IniParser, CommentsSkipped) {
    constexpr std::string_view input =
        "# comment line\n"
        "; another comment\n"
        "[Section]\n"
        "key=value\n"
        "# inline comment line\n"
        "key2=value2\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::none);
    ASSERT_EQ(cfg.size(), 1u);
    EXPECT_EQ(cfg.at("Section").size(), 2u);
}

TEST(IniParser, EmptyLinesSkipped) {
    constexpr std::string_view input =
        "\n\n\n"
        "[Section]\n"
        "\n"
        "key=value\n"
        "\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::none);
    EXPECT_EQ(cfg.at("Section").at("key"), "value");
}

TEST(IniParser, WhitespaceTrimmed) {
    constexpr std::string_view input =
        "[Section]\n"
        "  key  =  value  \n"
        "  other  =  data  \n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::none);
    EXPECT_EQ(cfg.at("Section").at("key"), "value");
    EXPECT_EQ(cfg.at("Section").at("other"), "data");
}

TEST(IniParser, EmptyValue) {
    constexpr std::string_view input =
        "[Section]\n"
        "key=\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::none);
    EXPECT_EQ(cfg.at("Section").at("key"), "");
}

TEST(IniParser, ValueWithEquals) {
    constexpr std::string_view input =
        "[Section]\n"
        "key=a=b=c\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::none);
    EXPECT_EQ(cfg.at("Section").at("key"), "a=b=c");
}

TEST(IniParser, EmptySectionCreated) {
    constexpr std::string_view input =
        "[EmptySection]\n"
        "[NonEmpty]\n"
        "key=val\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::none);
    EXPECT_EQ(cfg.size(), 2u);
    EXPECT_TRUE(cfg.at("EmptySection").empty());
}

TEST(IniParser, NoTrailingNewline) {
    constexpr std::string_view input =
        "[Section]\n"
        "key=value";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::none);
    EXPECT_EQ(cfg.at("Section").at("key"), "value");
}

// --- Error cases ---

TEST(IniParser, MalformedSectionHeader) {
    constexpr std::string_view input = "[Unclosed\nkey=val\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::config_parse_failed);
}

TEST(IniParser, EmptySectionName) {
    constexpr std::string_view input = "[]\nkey=val\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::config_parse_failed);
}

TEST(IniParser, KeyValueBeforeSection) {
    constexpr std::string_view input = "key=value\n[Section]\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::config_parse_failed);
}

TEST(IniParser, LineWithoutEquals) {
    constexpr std::string_view input = "[Section]\nnotakeyvalue\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::config_parse_failed);
}

TEST(IniParser, EmptyKey) {
    constexpr std::string_view input = "[Section]\n=value\n";

    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(input, cfg), Error::config_parse_failed);
}

// --- Compiled-in default config ---

TEST(IniParser, DefaultOverlayConfigParses) {
    ini::Config cfg;
    EXPECT_EQ(ini::parse_string(config::default_overlay_config, cfg), Error::none);
    ASSERT_EQ(cfg.count("ApplicationFolder"), 1u);
    EXPECT_FALSE(cfg.at("ApplicationFolder").empty());
}

TEST(IniParser, DefaultOverlayConfigContainsEtc) {
    ini::Config cfg;
    ASSERT_EQ(ini::parse_string(config::default_overlay_config, cfg), Error::none);

    const auto &section = cfg.at("ApplicationFolder");
    bool has_etc = false;
    for (const auto &[key, value] : section) {
        if (value == "/etc") {
            has_etc = true;
        }
    }
    EXPECT_TRUE(has_etc);
}

} // namespace
