
#include "executeNode.h"
#include "../FileSystem.h"

#include "gtest/gtest.h"
#include <fstream>

namespace
{
    using namespace YAM;


    TEST(FileSystem, createUniqueDirectory) {
        std::filesystem::path dir = FileSystem::createUniqueDirectory("__test");
        EXPECT_TRUE(std::filesystem::exists(dir));
        EXPECT_FALSE(std::filesystem::create_directory(dir));
        EXPECT_TRUE(dir.filename().string().starts_with("yam_"));
        EXPECT_TRUE(dir.filename().string().ends_with("__test"));
        std::filesystem::remove_all(dir);
    }
    TEST(FileSystem, uniquePath) {
        std::filesystem::path path = FileSystem::uniquePath();
        EXPECT_TRUE(path.filename().string().starts_with("yam_"));
        path = FileSystem::uniquePath(".postfix");
        EXPECT_TRUE(path.filename().string().starts_with("yam_"));
        EXPECT_TRUE(path.filename().string().ends_with(".postfix"));
    }

    TEST(FileSystem, normalizePath) {
        std::filesystem::path dir = FileSystem::createUniqueDirectory("__TEST");
        std::filesystem::path file(dir / "file.txt");
        std::ofstream stream(file.string().c_str());
        stream.close();
        std::filesystem::path notNorm(dir / "..\\." / dir.filename() / file.filename());
        std::filesystem::path norm = FileSystem::normalizePath(notNorm);
        EXPECT_EQ(file, norm);
        EXPECT_TRUE(norm.parent_path().string().ends_with("__test"));
        std::filesystem::remove_all(dir);
    }
}