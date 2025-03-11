#include "../MSBuildTrackerOutputReader.h"
#include "../FileSystem.h"

#include "gtest/gtest.h"

namespace
{
    using namespace YAM;
    using namespace std::filesystem;

    std::set<path> getExpectedReadFiles() {
        std::set<path> files;
        files.emplace(R"(D:\PETER\GITHUB\TUP\TUPFILE)");
        files.emplace(R"(D:\PETER\GITHUB\TUP\TUPRULES.TUP)");
        files.emplace(R"(D:\PETER\GITHUB\TUP\WIN32.TUP)");
        files.emplace(R"(D:\PETER\GITHUB\TUP\.TUP\DB-JOURNAL)");
        return files;
    }

    std::set<path> getExpectedWrittenFiles() {
        std::set<path> files;
        files.emplace(R"(D:\PETER\GITHUB\TUP\.TUP\DB-JOURNAL)");
        return files;
    }

    std::set<path> getExpectedReadOnlyFiles() {
        std::set<path> files;
        files.emplace(R"(D:\PETER\GITHUB\TUP\TUPFILE)");
        files.emplace(R"(D:\PETER\GITHUB\TUP\TUPRULES.TUP)");
        files.emplace(R"(D:\PETER\GITHUB\TUP\WIN32.TUP)");
        return files;
    }

    TEST(MSBuildTrackerOutputReader, read) {
        const path curDir = current_path();
        const path logDir(R"(..\..\core\coreTests\testData)");
        std::set<path> expectedReadFiles = getExpectedReadFiles();
        std::set<path> expectedWrittenFiles = getExpectedWrittenFiles();
        std::set<path> expectedReadOnlyFiles = getExpectedReadOnlyFiles();

        MSBuildTrackerOutputReader reader(logDir);
        EXPECT_EQ(expectedReadFiles, reader.readFiles());
        EXPECT_EQ(expectedWrittenFiles, reader.writtenFiles());
        EXPECT_EQ(expectedReadOnlyFiles, reader.readOnlyFiles());
    }
}