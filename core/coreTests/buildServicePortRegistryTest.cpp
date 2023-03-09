#include "../BuildServicePortRegistry.h"
#include "../FileSystem.h"
#include "../MemoryLogBook.h"
#include "../DotYamDirectory.h"

#include "gtest/gtest.h"

namespace
{
    using namespace YAM;

    class SetupYam
    {
    public:
        SetupYam() {
            MemoryLogBook logBook;
            std::filesystem::path repoDir(FileSystem::createUniqueDirectory());
            DotYamDirectory::initialize(repoDir, &logBook);
        }
    };

    TEST(BuildServicePortRegistry, writeAndRead) {
        SetupYam();

        boost::asio::ip::port_type port = 55330;
        BuildServicePortRegistry writer(port);
        ASSERT_TRUE(writer.good());
        EXPECT_TRUE(writer.serverRunning());

        BuildServicePortRegistry reader;
        ASSERT_TRUE(reader.good());
        EXPECT_EQ(port, reader.port());
        EXPECT_EQ(boost::interprocess::ipcdetail::get_current_process_id(), reader.pid());
        EXPECT_TRUE(reader.serverRunning());
    }

}