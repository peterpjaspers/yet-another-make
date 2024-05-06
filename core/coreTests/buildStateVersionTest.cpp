#include "../BuildStateVersion.h"
#include "../ExecutionContext.h"
#include "../FileSystem.h"
#include "../MemoryLogBook.h"

#include "gtest/gtest.h"
#include <fstream>

namespace
{
    using namespace YAM;

    void createFile(std::filesystem::path p) {
        std::ofstream stream(p);
        EXPECT_TRUE(stream.is_open());
        stream.close();
    }

    class TmpDir {
    public:
        std::filesystem::path dir;
        TmpDir() : dir(FileSystem::createUniqueDirectory()) {}
        ~TmpDir() { std::filesystem::remove_all(dir); }
    };
}

namespace
{
    TEST(BuildStateVersion, invalidWriteVersion) {
        EXPECT_ANY_THROW(BuildStateVersion::writeVersion(0));
    }

    TEST(BuildStateVersion, invalidReadVersionTooSmall) {
        EXPECT_ANY_THROW(BuildStateVersion::readableVersions({ 1, 0 }));
    }

    TEST(BuildStateVersion, invalidReadVersionTooLarge) {
        BuildStateVersion::writeVersion(3);
        EXPECT_ANY_THROW(BuildStateVersion::readableVersions({ 4 }));
    }

    TEST(BuildStateVersion, noFile) {
        TmpDir dir;
        MemoryLogBook logBook;

        BuildStateVersion::writeVersion(2);
        std::filesystem::path filePath = BuildStateVersion::select(dir.dir, logBook);
        EXPECT_EQ(dir.dir / "buildstate_2.bt", filePath);
        EXPECT_FALSE(std::filesystem::exists(dir.dir / "buildstate_2.bt"));
    }


    TEST(BuildStateVersion, illFormattedFile) {
        TmpDir dir;
        MemoryLogBook logBook;

        createFile(dir.dir / "buildstate_2.0.bt");
        BuildStateVersion::writeVersion(2);
        BuildStateVersion::readableVersions({ 1,2 });
        std::filesystem::path filePath = BuildStateVersion::select(dir.dir, logBook);
        EXPECT_EQ(dir.dir / "buildstate_2.bt", filePath);
        EXPECT_FALSE(std::filesystem::exists(dir.dir / "buildstate_2.bt"));
    }

    TEST(BuildStateVersion, upgradableFile) {
        TmpDir dir;
        MemoryLogBook logBook;

        std::filesystem::path readablePath(dir.dir / "buildstate_3.bt");
        createFile(readablePath);
        BuildStateVersion::writeVersion(4);
        BuildStateVersion::readableVersions({ 1,2,3});
        std::filesystem::path filePath = BuildStateVersion::select(dir.dir, logBook);
        std::filesystem::path expectedPath = dir.dir / "buildstate_4.bt";
        EXPECT_EQ(expectedPath, filePath);
        EXPECT_TRUE(std::filesystem::exists(readablePath));
        EXPECT_TRUE(std::filesystem::exists(expectedPath));
        auto record = logBook.records()[0];
        std::string expectedMsg = "The file is upgraded to " + expectedPath.string();
        EXPECT_NE(std::string::npos, record.message.find(expectedMsg));
    }

    TEST(BuildStateVersion, incompatibleFile) {
        TmpDir dir;
        MemoryLogBook logBook;

        std::filesystem::path incompatibleFile(dir.dir / "buildstate_1.bt");
        createFile(incompatibleFile);
        BuildStateVersion::writeVersion(4);
        BuildStateVersion::readableVersions({ 2,3 });
        std::filesystem::path filePath = BuildStateVersion::select(dir.dir, logBook);
        EXPECT_EQ("", filePath);
        auto record = logBook.records()[0];
        std::string expectedMsg = "Buildstate file " + incompatibleFile.string() + " has an incompatible version";
        EXPECT_NE(std::string::npos, record.message.find(expectedMsg));
    }

    TEST(BuildStateVersion, writableFile) {
        TmpDir dir;
        MemoryLogBook logBook;

        createFile(dir.dir / "buildstate_3.bt");
        BuildStateVersion::writeVersion(3);
        BuildStateVersion::readableVersions({ 3 });
        std::filesystem::path filePath = BuildStateVersion::select(dir.dir, logBook);
        EXPECT_EQ(dir.dir / "buildstate_3.bt", filePath);
        EXPECT_TRUE(std::filesystem::exists(dir.dir / "buildstate_3.bt"));
    }
}