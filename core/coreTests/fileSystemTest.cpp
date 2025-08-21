
#include "executeNode.h"
#include "../FileSystem.h"

#include "../../accessMonitor/Monitor.h"

#include "gtest/gtest.h"
#include <fstream>

#include "gtest/gtest.h"
#include <fstream>
#include <windows.h>

namespace
{
    using namespace YAM;

    class WorkingDir {
    public:
        std::filesystem::path dir;
        WorkingDir() : dir(FileSystem::createUniqueDirectory()) {}
        ~WorkingDir() { std::filesystem::remove_all(dir); }
    };


    TEST(FileSystem, createUniqueDirectory) {
        std::filesystem::path dir = FileSystem::createUniqueDirectory("__test");
        EXPECT_TRUE(std::filesystem::exists(dir));
        EXPECT_FALSE(std::filesystem::create_directory(dir));
        EXPECT_EQ(dir.parent_path().filename().string(), "yam_temp");
        EXPECT_TRUE(dir.filename().string().starts_with("__test"));
        std::filesystem::remove_all(dir);
    }
    TEST(FileSystem, uniquePath) {
        std::filesystem::path path = FileSystem::uniquePath();
        EXPECT_EQ(path.parent_path().filename().string(), "yam_temp");
        path = FileSystem::uniquePath(".prefix");
        EXPECT_EQ(path.parent_path().filename().string(), "yam_temp");
        EXPECT_TRUE(path.filename().string().starts_with(".prefix"));
    }

    TEST(FileSystem, canonicalPath) {
        std::filesystem::path dir = FileSystem::createUniqueDirectory("__TEST");
        std::filesystem::path file(dir / "file.txt");
        std::ofstream stream(file.string().c_str());
        stream.close();
        std::filesystem::path notNorm(dir / "..\\." / dir.filename() / file.filename());
        std::filesystem::path norm = FileSystem::canonicalPath(notNorm);
        EXPECT_EQ(file, norm);
        notNorm = FileSystem::toLower(notNorm);
        norm = FileSystem::canonicalPath(notNorm);
        EXPECT_EQ(file, norm);

        std::error_code ec;
        norm = std::filesystem::weakly_canonical("c:\\windows", ec);
        EXPECT_FALSE(ec);
        EXPECT_EQ(norm.string(), "C:\\Windows");

        norm = std::filesystem::weakly_canonical("c:\\windows\\JunkDir\\JunkFile", ec);
        EXPECT_EQ(norm.string(), "C:\\Windows\\JunkDir\\JunkFile");
        EXPECT_FALSE(ec);

        norm = std::filesystem::weakly_canonical("q:\\windows", ec);
        EXPECT_EQ(norm.string(), "q:\\windows");
        EXPECT_FALSE(ec);

        std::filesystem::remove_all(dir);
    }

    TEST(FileSystem, GetFullPathNameW) {
        WorkingDir tempDir;
        AccessMonitor::enableMonitoring();
        AccessMonitor::startMonitoring(tempDir.dir.string());
        {
            static const unsigned long MaxFileName = MAX_PATH;
            const wchar_t* fileName(L"c:\\Users\\Peter\\notExisting");
            wchar_t filePath[MaxFileName];
            wchar_t* fileNameAddress;
            DWORD length = GetFullPathNameW(fileName, MaxFileName, filePath, &fileNameAddress);
            std::wstring _fileName(filePath, length);
            std::cout << "GetFullPathNameW: " << std::filesystem::path(_fileName).string() << std::endl;

            std::error_code ec;
            auto norm = std::filesystem::weakly_canonical(fileName, ec);
            std::cout << "weakly_canonical: " << std::filesystem::path(norm).string() << std::endl;
        }
        AccessMonitor::MonitorEvents unfilteredResult;
        AccessMonitor::stopMonitoring(&unfilteredResult);
        AccessMonitor::disableMonitoring();
    }

    TEST(FileSystem, toLower) {
        std::filesystem::path path("SOMEdir/File.txt");
        std::filesystem::path lower = FileSystem::toLower(path);
        EXPECT_EQ("somedir/file.txt", lower.string());
        EXPECT_EQ(std::filesystem::path("somedir/file.txt"), lower);
        EXPECT_NE(path, lower);
    }
}