#include <gtest/gtest.h>
#include "mount.h"

namespace {

// --- OverlayDescription::Persistent ---

TEST(PersistentDesc, DefaultConstructorEmpty) {
    OverlayDescription::Persistent desc;
    EXPECT_TRUE(desc.lower_directory.empty());
    EXPECT_TRUE(desc.upper_directory.empty());
    EXPECT_TRUE(desc.work_directory.empty());
    EXPECT_TRUE(desc.merge_directory.empty());
}

TEST(PersistentDesc, CopyConstructor) {
    OverlayDescription::Persistent orig;
    orig.lower_directory = "/lower";
    orig.upper_directory = "/upper";
    orig.work_directory = "/work";
    orig.merge_directory = "/merge";

    OverlayDescription::Persistent copy(orig);
    EXPECT_EQ(copy.lower_directory, "/lower");
    EXPECT_EQ(copy.upper_directory, "/upper");
    EXPECT_EQ(copy.work_directory, "/work");
    EXPECT_EQ(copy.merge_directory, "/merge");
}

TEST(PersistentDesc, MoveConstructor) {
    OverlayDescription::Persistent orig;
    orig.lower_directory = "/lower";
    orig.merge_directory = "/merge";

    OverlayDescription::Persistent moved(std::move(orig));
    EXPECT_EQ(moved.lower_directory, "/lower");
    EXPECT_EQ(moved.merge_directory, "/merge");
}

TEST(PersistentDesc, EqualityOperator) {
    OverlayDescription::Persistent a;
    a.lower_directory = "/lower";
    a.upper_directory = "/upper";
    a.work_directory = "/work";
    a.merge_directory = "/merge";

    OverlayDescription::Persistent b = a;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);

    b.upper_directory = "/different";
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(PersistentDesc, DefaultNosuidTrue) {
    OverlayDescription::Persistent desc;
    EXPECT_TRUE(desc.nosuid);
}

TEST(PersistentDesc, EqualityIncludesNosuid) {
    OverlayDescription::Persistent a;
    a.lower_directory = "/lower";
    a.upper_directory = "/upper";
    a.work_directory = "/work";
    a.merge_directory = "/merge";

    OverlayDescription::Persistent b = a;
    EXPECT_TRUE(a == b);

    b.nosuid = false;
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

// --- OverlayDescription::ReadOnly ---

TEST(ReadOnlyDesc, DefaultConstructorEmpty) {
    OverlayDescription::ReadOnly desc;
    EXPECT_TRUE(desc.lower_directory.empty());
    EXPECT_TRUE(desc.merge_directory.empty());
}

TEST(ReadOnlyDesc, CopyConstructor) {
    OverlayDescription::ReadOnly orig;
    orig.lower_directory = "/lower";
    orig.merge_directory = "/merge";

    OverlayDescription::ReadOnly copy(orig);
    EXPECT_EQ(copy.lower_directory, "/lower");
    EXPECT_EQ(copy.merge_directory, "/merge");
}

TEST(ReadOnlyDesc, MoveConstructor) {
    OverlayDescription::ReadOnly orig;
    orig.lower_directory = "/lower";
    orig.merge_directory = "/merge";

    OverlayDescription::ReadOnly moved(std::move(orig));
    EXPECT_EQ(moved.lower_directory, "/lower");
    EXPECT_EQ(moved.merge_directory, "/merge");
}

TEST(ReadOnlyDesc, EqualityOperator) {
    OverlayDescription::ReadOnly a;
    a.lower_directory = "/lower";
    a.merge_directory = "/merge";

    OverlayDescription::ReadOnly b = a;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);

    b.lower_directory = "/different";
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

} // namespace
