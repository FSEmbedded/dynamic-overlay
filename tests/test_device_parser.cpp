#include <gtest/gtest.h>
#include "device_parser.h"

namespace {

using PersistentMemDetector::detail::parse_emmc_device;
using PersistentMemDetector::detail::parse_nand_device;

// --- parse_emmc_device ---

TEST(ParseEmmcDevice, ValidMmcblk0) {
    std::string out;
    EXPECT_TRUE(parse_emmc_device("root=/dev/mmcblk0p2 console=ttymxc0", out));
    EXPECT_EQ(out, "mmcblk0");
}

TEST(ParseEmmcDevice, ValidMmcblk1) {
    std::string out;
    EXPECT_TRUE(parse_emmc_device("root=/dev/mmcblk1p1", out));
    EXPECT_EQ(out, "mmcblk1");
}

TEST(ParseEmmcDevice, ValidMmcblk2) {
    std::string out;
    EXPECT_TRUE(parse_emmc_device("root=/dev/mmcblk2p3", out));
    EXPECT_EQ(out, "mmcblk2");
}

TEST(ParseEmmcDevice, NoMatch) {
    std::string out;
    EXPECT_FALSE(parse_emmc_device("root=/dev/sda1 console=ttyS0", out));
}

TEST(ParseEmmcDevice, EmptyInput) {
    std::string out;
    EXPECT_FALSE(parse_emmc_device("", out));
}

TEST(ParseEmmcDevice, NandInputNoMatch) {
    std::string out;
    EXPECT_FALSE(parse_emmc_device("root=/dev/ubiblock0_0", out));
}

TEST(ParseEmmcDevice, InvalidDeviceNumber) {
    std::string out;
    // mmcblk3 is out of range (0-2 only)
    EXPECT_FALSE(parse_emmc_device("root=/dev/mmcblk3p1", out));
}

TEST(ParseEmmcDevice, PrefixInMiddleOfCmdline) {
    std::string out;
    EXPECT_TRUE(parse_emmc_device(
        "console=ttymxc0,115200 root=/dev/mmcblk0p2 rootwait", out));
    EXPECT_EQ(out, "mmcblk0");
}

// --- parse_nand_device ---

TEST(ParseNandDevice, ValidUbiblock) {
    std::string out;
    EXPECT_TRUE(parse_nand_device("root=/dev/ubiblock0_0 ubi.mtd=1", out));
    EXPECT_EQ(out, "ubiblock0_0");
}

TEST(ParseNandDevice, DifferentUbiNumbers) {
    std::string out;
    EXPECT_TRUE(parse_nand_device("root=/dev/ubiblock1_2", out));
    EXPECT_EQ(out, "ubiblock1_2");
}

TEST(ParseNandDevice, NoMatch) {
    std::string out;
    EXPECT_FALSE(parse_nand_device("root=/dev/mmcblk0p1", out));
}

TEST(ParseNandDevice, EmptyInput) {
    std::string out;
    EXPECT_FALSE(parse_nand_device("", out));
}

TEST(ParseNandDevice, PrefixInMiddleOfCmdline) {
    std::string out;
    EXPECT_TRUE(parse_nand_device(
        "console=ttymxc0 root=/dev/ubiblock0_0 rootwait", out));
    EXPECT_EQ(out, "ubiblock0_0");
}

TEST(ParseNandDevice, StopsAtWhitespace) {
    std::string out;
    EXPECT_TRUE(parse_nand_device("root=/dev/ubiblock0_0 extra", out));
    EXPECT_EQ(out, "ubiblock0_0");
}

} // namespace
