#include "../CollapsedFileChanges.h"

#include "gtest/gtest.h"

namespace
{
    using namespace YAM;
    using Action = FileChange::Action;

    std::chrono::time_point<std::chrono::utc_clock> lwt() {
        return std::chrono::utc_clock::now();
    }
    std::chrono::milliseconds offset(10);

    std::filesystem::path rootDir("root");
    std::filesystem::path file1("junk");
    std::filesystem::path file2("aap");
    std::filesystem::path absFile1(rootDir / file1);
    std::filesystem::path absFile2(rootDir / file2);

    FileChange add1{ Action::Added, absFile1, std::filesystem::path(), lwt() + offset };
    FileChange add2{ Action::Added, absFile2, std::filesystem::path(), lwt() + 2*offset };
    FileChange modify1{ Action::Modified, absFile1, std::filesystem::path(), lwt() + 3*offset };
    FileChange modify2{ Action::Modified, absFile2, std::filesystem::path(), lwt() + 4*offset };
    FileChange remove1{ Action::Removed, absFile1, std::filesystem::path(), lwt() + 5*offset };
    FileChange remove2{ Action::Removed, absFile2, std::filesystem::path(), lwt() + 5*offset };
    FileChange rename1to2{ Action::Renamed, absFile2, absFile1, lwt() + 7*offset };
    FileChange rename2to1{ Action::Renamed, absFile1, absFile2, lwt() + 8*offset };

    class Helper
    {
    public:
        CollapsedFileChanges changes;

        Helper() {}

        FileChange find(std::filesystem::path const& path) {
            auto cmap = changes.changes();
            auto it = cmap.find(path);
            // apparently ASSERT_NE(cmap.end(), it) only compiles in TEST sections.
            if (cmap.end() == it) throw std::exception("not found");
            return it->second;
        }

        void add(FileChange const& change) {
            changes.add(change);
        }

        void assertSize(std::size_t expected) {
            ASSERT_EQ(expected, changes.changes().size());
        }
    };

    TEST(CollapsedFileChanges, add1) {
        Helper helper;
        helper.add(add1);

        helper.assertSize(1);
        auto const& actual1 = helper.find(absFile1);

        EXPECT_EQ(Action::Added, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(add1.lastWriteTime, actual1.lastWriteTime);
    }

    TEST(CollapsedFileChanges, remove1) {
        Helper helper;
        helper.add(remove1);

        helper.assertSize(1);
        auto const& actual1 = helper.find(absFile1);

        EXPECT_EQ(Action::Removed, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(remove1.lastWriteTime, actual1.lastWriteTime);
    }

    TEST(CollapsedFileChanges, modified) {
        Helper helper;
        helper.add(modify1);

        helper.assertSize(1);
        auto const& actual1 = helper.find(absFile1);

        EXPECT_EQ(Action::Modified, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(modify1.lastWriteTime, actual1.lastWriteTime);
    }

    TEST(CollapsedFileChanges, rename1to2) {
        Helper helper;
        helper.add(rename1to2);

        helper.assertSize(2);
        auto const& actual1 = helper.find(absFile1);
        auto const& actual2 = helper.find(absFile2);

        EXPECT_EQ(Action::Removed, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(rename1to2.lastWriteTime, actual1.lastWriteTime);
        EXPECT_EQ(Action::Added, actual2.action);
        EXPECT_EQ(absFile2.string(), actual2.fileName.string());
        EXPECT_EQ(rename1to2.lastWriteTime, actual2.lastWriteTime);
    }

    TEST(CollapsedFileChanges, add1_add1) {
        Helper helper;
        helper.add(add1);
        helper.add(add1);

        helper.assertSize(1);
        auto const& actual1 = helper.find(absFile1);

        EXPECT_EQ(Action::Added, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(add1.lastWriteTime, actual1.lastWriteTime);
    }
    TEST(CollapsedFileChanges, add1_remove1) {
        Helper helper;
        helper.add(add1);
        helper.add(remove1);

        helper.assertSize(1);
        auto const& actual1 = helper.find(absFile1);

        EXPECT_EQ(Action::Removed, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(remove1.lastWriteTime, actual1.lastWriteTime);
    }

    TEST(CollapsedFileChanges, add1_modify1) {
        Helper helper;
        helper.add(add1);
        helper.add(modify1);

        helper.assertSize(1);
        auto const& actual1 = helper.find(absFile1);

        EXPECT_EQ(Action::Added, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(modify1.lastWriteTime, actual1.lastWriteTime);
    }
    TEST(CollapsedFileChanges, add1_rename1to2) {
        Helper helper;
        helper.add(add1);
        helper.add(rename1to2);

        helper.assertSize(2);
        auto const& actual1 = helper.find(absFile1);
        auto const& actual2 = helper.find(absFile2);

        EXPECT_EQ(Action::Removed, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(rename1to2.lastWriteTime, actual1.lastWriteTime);

        EXPECT_EQ(Action::Added, actual2.action);
        EXPECT_EQ(absFile2.string(), actual2.fileName.string());
        EXPECT_EQ(rename1to2.lastWriteTime, actual2.lastWriteTime);
    }

    TEST(CollapsedFileChanges, modify1_add1) {
        Helper helper;
        helper.add(modify1);
        helper.add(add1);

        helper.assertSize(1);
        auto const& actual1 = helper.find(absFile1);

        EXPECT_EQ(Action::Added, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(add1.lastWriteTime, actual1.lastWriteTime);
    }

    TEST(CollapsedFileChanges, modify1_remove1) {
        Helper helper;
        helper.add(modify1);
        helper.add(remove1);

        helper.assertSize(1);
        auto const& actual1 = helper.find(absFile1);

        EXPECT_EQ(Action::Removed, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(remove1.lastWriteTime, actual1.lastWriteTime);
    }

    TEST(CollapsedFileChanges, modify1_modify1) {
        Helper helper;
        FileChange mod1 = modify1;
        mod1.lastWriteTime += offset;
        helper.add(modify1);
        helper.add(mod1);

        helper.assertSize(1);
        auto const& actual1 = helper.find(absFile1);

        EXPECT_EQ(Action::Modified, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(mod1.lastWriteTime, actual1.lastWriteTime);
    }
    TEST(CollapsedFileChanges, modify1_rename1to2) {
        Helper helper;
        helper.add(modify1);
        helper.add(rename1to2);

        helper.assertSize(2);
        auto const& actual1 = helper.find(absFile1);
        auto const& actual2 = helper.find(absFile2);

        EXPECT_EQ(Action::Removed, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(rename1to2.lastWriteTime, actual1.lastWriteTime);

        EXPECT_EQ(Action::Added, actual2.action);
        EXPECT_EQ(absFile2.string(), actual2.fileName.string());
        EXPECT_EQ(rename1to2.lastWriteTime, actual2.lastWriteTime);
    }

    TEST(CollapsedFileChanges, remove1_add1) {
        Helper helper;
        helper.add(remove1);
        helper.add(add1);

        helper.assertSize(1);
        auto const& actual1 = helper.find(absFile1);

        EXPECT_EQ(Action::Added, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(add1.lastWriteTime, actual1.lastWriteTime);
    }

    TEST(CollapsedFileChanges, remove1_remove1) {
        Helper helper;
        FileChange rem1 = remove1;
        rem1.lastWriteTime += offset;
        helper.add(remove1);
        helper.add(rem1);

        helper.assertSize(1);
        auto const& actual1 = helper.find(absFile1);

        EXPECT_EQ(Action::Removed, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(rem1.lastWriteTime, actual1.lastWriteTime);
    }

    TEST(CollapsedFileChanges, remove1_modify1) {
        Helper helper;
        helper.add(remove1);
        helper.add(modify1);

        helper.assertSize(1);
        auto const& actual1 = helper.find(absFile1);

        EXPECT_EQ(Action::Removed, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(modify1.lastWriteTime, actual1.lastWriteTime);
    }
    TEST(CollapsedFileChanges, remove1_rename1to2) {
        Helper helper;
        helper.add(remove1);
        helper.add(rename1to2);

        helper.assertSize(2);
        auto const& actual1 = helper.find(absFile1);
        auto const& actual2 = helper.find(absFile2);

        EXPECT_EQ(Action::Removed, actual1.action);
        EXPECT_EQ(absFile1.string(), actual1.fileName.string());
        EXPECT_EQ(rename1to2.lastWriteTime, actual1.lastWriteTime);

        EXPECT_EQ(Action::Added, actual2.action);
        EXPECT_EQ(absFile2.string(), actual2.fileName.string());
        EXPECT_EQ(rename1to2.lastWriteTime, actual2.lastWriteTime);
    }
}