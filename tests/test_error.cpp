#include <gtest/gtest.h>
#include "error.h"

namespace {

TEST(ErrorEnum, NoneIsZero) {
    EXPECT_EQ(static_cast<uint8_t>(Error::none), 0u);
}

TEST(ErrorEnum, StableAbiValues) {
    // IEC 62304: error codes are ABI-stable — do not renumber existing values
    EXPECT_EQ(static_cast<uint8_t>(Error::mount_failed), 1u);
    EXPECT_EQ(static_cast<uint8_t>(Error::umount_failed), 2u);
    EXPECT_EQ(static_cast<uint8_t>(Error::loop_device_failed), 3u);
    EXPECT_EQ(static_cast<uint8_t>(Error::overlay_mount_failed), 4u);
    EXPECT_EQ(static_cast<uint8_t>(Error::directory_create_failed), 5u);
    EXPECT_EQ(static_cast<uint8_t>(Error::config_invalid), 6u);
    EXPECT_EQ(static_cast<uint8_t>(Error::config_not_found), 7u);
    EXPECT_EQ(static_cast<uint8_t>(Error::config_parse_failed), 8u);
    EXPECT_EQ(static_cast<uint8_t>(Error::uboot_init_failed), 9u);
    EXPECT_EQ(static_cast<uint8_t>(Error::uboot_read_failed), 10u);
    EXPECT_EQ(static_cast<uint8_t>(Error::uboot_var_not_found), 11u);
    EXPECT_EQ(static_cast<uint8_t>(Error::uboot_var_invalid), 12u);
    EXPECT_EQ(static_cast<uint8_t>(Error::permission_denied), 13u);
    EXPECT_EQ(static_cast<uint8_t>(Error::stat_failed), 14u);
    EXPECT_EQ(static_cast<uint8_t>(Error::chmod_failed), 15u);
    EXPECT_EQ(static_cast<uint8_t>(Error::chown_failed), 16u);
    EXPECT_EQ(static_cast<uint8_t>(Error::open_failed), 17u);
    EXPECT_EQ(static_cast<uint8_t>(Error::read_failed), 18u);
    EXPECT_EQ(static_cast<uint8_t>(Error::write_failed), 19u);
    EXPECT_EQ(static_cast<uint8_t>(Error::close_failed), 20u);
    EXPECT_EQ(static_cast<uint8_t>(Error::path_not_found), 21u);
    EXPECT_EQ(static_cast<uint8_t>(Error::memory_detect_failed), 22u);
    EXPECT_EQ(static_cast<uint8_t>(Error::blkid_failed), 23u);
    EXPECT_EQ(static_cast<uint8_t>(Error::symlink_failed), 24u);
    EXPECT_EQ(static_cast<uint8_t>(Error::copy_failed), 25u);
    EXPECT_EQ(static_cast<uint8_t>(Error::rename_failed), 26u);
    EXPECT_EQ(static_cast<uint8_t>(Error::already_mounted), 27u);
    EXPECT_EQ(static_cast<uint8_t>(Error::not_mounted), 28u);
    EXPECT_EQ(static_cast<uint8_t>(Error::max_overlay_depth), 29u);
    EXPECT_EQ(static_cast<uint8_t>(Error::invalid_argument), 30u);
    EXPECT_EQ(static_cast<uint8_t>(Error::cert_store_failed), 31u);
    EXPECT_EQ(static_cast<uint8_t>(Error::json_parse_failed), 32u);
    EXPECT_EQ(static_cast<uint8_t>(Error::remove_failed), 33u);
}

TEST(ErrorEnum, CountGuard) {
    // If you add a new error code, update StableAbiValues and AllCodesHaveExplicitStrings.
    // The highest value + 1 must equal the expected count.
    EXPECT_EQ(static_cast<uint8_t>(Error::remove_failed) + 1, 34u);
}

TEST(ErrorToString, NoneReturnsNoError) {
    EXPECT_EQ(error_to_string(Error::none), "no error");
}

TEST(ErrorToString, KnownCodesReturnDescriptiveString) {
    EXPECT_EQ(error_to_string(Error::mount_failed), "mount failed");
    EXPECT_EQ(error_to_string(Error::config_invalid), "configuration invalid");
    EXPECT_EQ(error_to_string(Error::uboot_init_failed), "U-Boot init failed");
    EXPECT_EQ(error_to_string(Error::cert_store_failed), "certificate store operation failed");
}

TEST(ErrorToString, AllCodesHaveExplicitStrings) {
    // Every defined error code should have a specific message, not "unknown error"
    constexpr std::string_view unknown = "unknown error";

    EXPECT_NE(error_to_string(Error::none), unknown);
    EXPECT_NE(error_to_string(Error::mount_failed), unknown);
    EXPECT_NE(error_to_string(Error::umount_failed), unknown);
    EXPECT_NE(error_to_string(Error::loop_device_failed), unknown);
    EXPECT_NE(error_to_string(Error::overlay_mount_failed), unknown);
    EXPECT_NE(error_to_string(Error::directory_create_failed), unknown);
    EXPECT_NE(error_to_string(Error::config_invalid), unknown);
    EXPECT_NE(error_to_string(Error::config_not_found), unknown);
    EXPECT_NE(error_to_string(Error::config_parse_failed), unknown);
    EXPECT_NE(error_to_string(Error::uboot_init_failed), unknown);
    EXPECT_NE(error_to_string(Error::uboot_read_failed), unknown);
    EXPECT_NE(error_to_string(Error::uboot_var_not_found), unknown);
    EXPECT_NE(error_to_string(Error::uboot_var_invalid), unknown);
    EXPECT_NE(error_to_string(Error::permission_denied), unknown);
    EXPECT_NE(error_to_string(Error::stat_failed), unknown);
    EXPECT_NE(error_to_string(Error::chmod_failed), unknown);
    EXPECT_NE(error_to_string(Error::chown_failed), unknown);
    EXPECT_NE(error_to_string(Error::open_failed), unknown);
    EXPECT_NE(error_to_string(Error::read_failed), unknown);
    EXPECT_NE(error_to_string(Error::write_failed), unknown);
    EXPECT_NE(error_to_string(Error::close_failed), unknown);
    EXPECT_NE(error_to_string(Error::path_not_found), unknown);
    EXPECT_NE(error_to_string(Error::memory_detect_failed), unknown);
    EXPECT_NE(error_to_string(Error::blkid_failed), unknown);
    EXPECT_NE(error_to_string(Error::symlink_failed), unknown);
    EXPECT_NE(error_to_string(Error::copy_failed), unknown);
    EXPECT_NE(error_to_string(Error::rename_failed), unknown);
    EXPECT_NE(error_to_string(Error::already_mounted), unknown);
    EXPECT_NE(error_to_string(Error::not_mounted), unknown);
    EXPECT_NE(error_to_string(Error::max_overlay_depth), unknown);
    EXPECT_NE(error_to_string(Error::invalid_argument), unknown);
    EXPECT_NE(error_to_string(Error::cert_store_failed), unknown);
    EXPECT_NE(error_to_string(Error::json_parse_failed), unknown);
    EXPECT_NE(error_to_string(Error::remove_failed), unknown);
}

} // namespace
