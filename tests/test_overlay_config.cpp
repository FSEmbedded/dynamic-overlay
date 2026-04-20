#include <gtest/gtest.h>

#include "overlay_config.h"
#include "mount.h"
#include "error.h"
#include "ini_parser.h"

namespace {

ini::Section make_valid_section()
{
    ini::Section s;
    s["lowerdir"]  = "/etc";
    s["upperdir"]  = "/rw_fs/root/upper/etc";
    s["workdir"]   = "/rw_fs/root/work/etc";
    s["mergedir"]  = "/etc";
    return s;
}

TEST(OverlayConfig, RequiredFieldsParsed) {
    OverlayDescription::Persistent out;
    const auto err = overlay_config::parse_persistent_section(
        "PersistentMemory.etc", make_valid_section(), out);

    EXPECT_EQ(err, Error::none);
    EXPECT_EQ(out.lower_directory, "/etc");
    EXPECT_EQ(out.upper_directory, "/rw_fs/root/upper/etc");
    EXPECT_EQ(out.work_directory,  "/rw_fs/root/work/etc");
    EXPECT_EQ(out.merge_directory, "/etc");
}

TEST(OverlayConfig, NosuidDefaultsTrueWhenAbsent) {
    OverlayDescription::Persistent out;
    ASSERT_EQ(overlay_config::parse_persistent_section(
        "PersistentMemory.etc", make_valid_section(), out), Error::none);
    EXPECT_TRUE(out.nosuid);
}

TEST(OverlayConfig, NosuidExplicitTrue) {
    auto s = make_valid_section();
    s["nosuid"] = "true";
    OverlayDescription::Persistent out;
    ASSERT_EQ(overlay_config::parse_persistent_section(
        "PersistentMemory.etc", s, out), Error::none);
    EXPECT_TRUE(out.nosuid);
}

TEST(OverlayConfig, NosuidExplicitFalseOptsOut) {
    auto s = make_valid_section();
    s["nosuid"] = "false";
    OverlayDescription::Persistent out;
    ASSERT_EQ(overlay_config::parse_persistent_section(
        "PersistentMemory.usr/bin", s, out), Error::none);
    EXPECT_FALSE(out.nosuid);
}

TEST(OverlayConfig, NosuidInvalidValueRejected) {
    auto s = make_valid_section();
    s["nosuid"] = "maybe";
    OverlayDescription::Persistent out;
    EXPECT_EQ(overlay_config::parse_persistent_section(
        "PersistentMemory.etc", s, out), Error::config_invalid);
}

TEST(OverlayConfig, NosuidEmptyValueRejected) {
    auto s = make_valid_section();
    s["nosuid"] = "";
    OverlayDescription::Persistent out;
    EXPECT_EQ(overlay_config::parse_persistent_section(
        "PersistentMemory.etc", s, out), Error::config_invalid);
}

TEST(OverlayConfig, NosuidCaseSensitive) {
    auto s = make_valid_section();
    s["nosuid"] = "True";
    OverlayDescription::Persistent out;
    EXPECT_EQ(overlay_config::parse_persistent_section(
        "PersistentMemory.etc", s, out), Error::config_invalid);
}

TEST(OverlayConfig, UnknownKeyRejected) {
    auto s = make_valid_section();
    s["suid"] = "false";
    OverlayDescription::Persistent out;
    EXPECT_EQ(overlay_config::parse_persistent_section(
        "PersistentMemory.etc", s, out), Error::config_invalid);
}

TEST(OverlayConfig, MissingLowerdirRejected) {
    auto s = make_valid_section();
    s.erase("lowerdir");
    OverlayDescription::Persistent out;
    EXPECT_EQ(overlay_config::parse_persistent_section(
        "PersistentMemory.etc", s, out), Error::config_invalid);
}

TEST(OverlayConfig, MissingUpperdirRejected) {
    auto s = make_valid_section();
    s.erase("upperdir");
    OverlayDescription::Persistent out;
    EXPECT_EQ(overlay_config::parse_persistent_section(
        "PersistentMemory.etc", s, out), Error::config_invalid);
}

TEST(OverlayConfig, MissingWorkdirRejected) {
    auto s = make_valid_section();
    s.erase("workdir");
    OverlayDescription::Persistent out;
    EXPECT_EQ(overlay_config::parse_persistent_section(
        "PersistentMemory.etc", s, out), Error::config_invalid);
}

TEST(OverlayConfig, MissingMergedirRejected) {
    auto s = make_valid_section();
    s.erase("mergedir");
    OverlayDescription::Persistent out;
    EXPECT_EQ(overlay_config::parse_persistent_section(
        "PersistentMemory.etc", s, out), Error::config_invalid);
}

TEST(OverlayConfig, EmptySectionRejected) {
    ini::Section s;
    OverlayDescription::Persistent out;
    EXPECT_EQ(overlay_config::parse_persistent_section(
        "PersistentMemory.etc", s, out), Error::config_invalid);
}

} // namespace
