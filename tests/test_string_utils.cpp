#include <gtest/gtest.h>
#include "string_utils.h"

namespace {

// --- trim ---

TEST(Trim, EmptyString) {
    EXPECT_EQ(string_utils::trim(""), "");
}

TEST(Trim, NoWhitespace) {
    EXPECT_EQ(string_utils::trim("hello"), "hello");
}

TEST(Trim, LeadingSpaces) {
    EXPECT_EQ(string_utils::trim("  hello"), "hello");
}

TEST(Trim, TrailingSpaces) {
    EXPECT_EQ(string_utils::trim("hello  "), "hello");
}

TEST(Trim, BothSides) {
    EXPECT_EQ(string_utils::trim("  hello  "), "hello");
}

TEST(Trim, Tabs) {
    EXPECT_EQ(string_utils::trim("\thello\t"), "hello");
}

TEST(Trim, MixedWhitespace) {
    EXPECT_EQ(string_utils::trim(" \t\r\nhello\r\n\t "), "hello");
}

TEST(Trim, OnlyWhitespace) {
    EXPECT_EQ(string_utils::trim("   \t\r\n"), "");
}

TEST(Trim, InternalWhitespacePreserved) {
    EXPECT_EQ(string_utils::trim("  hello world  "), "hello world");
}

// --- to_lower ---

TEST(ToLower, EmptyString) {
    EXPECT_EQ(string_utils::to_lower(""), "");
}

TEST(ToLower, AlreadyLower) {
    EXPECT_EQ(string_utils::to_lower("hello"), "hello");
}

TEST(ToLower, AllUpper) {
    EXPECT_EQ(string_utils::to_lower("HELLO"), "hello");
}

TEST(ToLower, MixedCase) {
    EXPECT_EQ(string_utils::to_lower("HeLLo WoRLd"), "hello world");
}

TEST(ToLower, NonAlphaPreserved) {
    EXPECT_EQ(string_utils::to_lower("/dev/mmcblk0p1"), "/dev/mmcblk0p1");
}

// --- split ---

TEST(Split, EmptyString) {
    auto result = string_utils::split("", ':');
    EXPECT_TRUE(result.empty());
}

TEST(Split, SingleElement) {
    auto result = string_utils::split("/etc", ':');
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "/etc");
}

TEST(Split, MultipleElements) {
    auto result = string_utils::split("/etc:/usr:/var", ':');
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "/etc");
    EXPECT_EQ(result[1], "/usr");
    EXPECT_EQ(result[2], "/var");
}

TEST(Split, TrailingDelimiter) {
    auto result = string_utils::split("/etc:/usr:", ':');
    // Implementation consumes trailing delimiter without producing empty element
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "/etc");
    EXPECT_EQ(result[1], "/usr");
}

TEST(Split, EmptyBetweenDelimiters) {
    auto result = string_utils::split("/a::/b", ':');
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "/a");
    EXPECT_EQ(result[1], "");
    EXPECT_EQ(result[2], "/b");
}

// --- join ---

TEST(Join, EmptyVector) {
    EXPECT_EQ(string_utils::join({}, ':'), "");
}

TEST(Join, SingleElement) {
    EXPECT_EQ(string_utils::join({"/etc"}, ':'), "/etc");
}

TEST(Join, MultipleElements) {
    EXPECT_EQ(string_utils::join({"/etc", "/usr", "/var"}, ':'), "/etc:/usr:/var");
}

TEST(Join, RoundTrip) {
    const std::string original = "/etc:/usr:/var";
    auto parts = string_utils::split(original, ':');
    EXPECT_EQ(string_utils::join(parts, ':'), original);
}

} // namespace
