#include "../MonitoredProcessWin32.h"
#include "../FileSystem.h"

#include "gtest/gtest.h"
#include <chrono>
#include <string>
#include <vector>
#include <thread>
#include <boost/process.hpp>
#include <fstream>

namespace
{
    using namespace YAM;

    class WorkingDir {
    public:
        std::filesystem::path dir;
        WorkingDir() : dir(FileSystem::createUniqueDirectory()) {}
        ~WorkingDir() { std::filesystem::remove_all(dir); }
    };

    std::filesystem::path wdir(std::filesystem::current_path());
    TEST(MonitoredProcessWin32, ping) {
        std::map<std::string, std::string> env;

        auto pingExe = boost::process::search_path("ping").string();
        MonitoredProcessWin32 ping(pingExe, "-n 3 127.0.0.1", wdir, env);
        //ping will take approximately n (==3) seconds. Use larger timeout.
        EXPECT_TRUE(ping.wait_for(15000));
        MonitoredProcessResult result = ping.wait();
        ASSERT_EQ(0, result.exitCode);

        std::vector<std::string> stdoutLines;
        result.toLines(result.stdOut, stdoutLines);
        unsigned int nReplies = 0;
        for (auto const& line : stdoutLines) {
            if (line.starts_with("Reply from")) nReplies++;
        }
        EXPECT_EQ(3, nReplies);
    }

    TEST(MonitoredProcessWin32, captureStdOutAndStderr) {
        std::map<std::string, std::string> env;

        auto cmdExe = boost::process::search_path("cmd").string();
        MonitoredProcessWin32 cmd(cmdExe, "/c dir notlikelytoexist.blabla", wdir, env);
        EXPECT_TRUE(cmd.wait_for(15000));
        MonitoredProcessResult result = cmd.wait();
        ASSERT_EQ(1, result.exitCode);
        std::string expected("File Not Found");
        EXPECT_EQ(std::string::npos, result.stdOut.find(expected));
        EXPECT_EQ(0, result.stdErr.find(expected));
    }

    TEST(MonitoredProcessWin32, passEnvironment) {
        std::map<std::string, std::string> env;
        env["rubbish"] = "nonsense";

        auto cmdExe = boost::process::search_path("cmd").string();
        MonitoredProcessWin32 cmd(cmdExe, "/c echo %rubbish% > c:\\temp\\junk.txt & type c:\\temp\\junk.txt", wdir, env);
        EXPECT_TRUE(cmd.wait_for(15000));
        MonitoredProcessResult result = cmd.wait();
        ASSERT_EQ(0, result.exitCode);
        std::string expected("nonsense  \r\n");
        EXPECT_EQ(expected, result.stdOut);
    }

    void terminate(MonitoredProcessWin32& p) {
        // wait a bit to allow terminate test to block on ping.wait().
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        p.terminate();
    }
    TEST(MonitoredProcessWin32, terminate) {
        std::map<std::string, std::string> env;

        auto pingExe = boost::process::search_path("ping").string();
        MonitoredProcessWin32 ping(pingExe, "-n 3000 127.0.0.1", wdir, env);
        std::thread t(terminate, std::ref(ping));
        MonitoredProcessResult result = ping.wait();
        ASSERT_EQ(1, result.exitCode);
        t.join();
    }

    TEST(MonitoredProcessWin32, fileDependencies) {
        WorkingDir tempDir;
        std::map<std::string, std::string> env;
        std::filesystem::path junkFile(tempDir.dir / "junk.txt");
        auto readJunkFile = [&]() {
            std::ifstream istream(junkFile);
            std::string result;
            istream >> result;
            istream.close();
            return result;
        };

        auto cmdExe = boost::process::search_path("cmd").string();
        MonitoredProcessWin32 cmd(
            cmdExe, 
            "/c echo rubbish > junk.txt & type junk.txt", 
            tempDir.dir, 
            env);
        EXPECT_TRUE(cmd.wait_for(15000));
        MonitoredProcessResult result = cmd.wait();
        ASSERT_EQ(0, result.exitCode);
        EXPECT_EQ(1, result.readFiles.size());
        EXPECT_TRUE(result.readFiles.contains(junkFile));
        EXPECT_EQ(1, result.writtenFiles.size());
        EXPECT_TRUE(result.writtenFiles.contains(junkFile));
        EXPECT_EQ(0, result.readOnlyFiles.size());
        EXPECT_EQ(std::string("rubbish"), readJunkFile());
    }
}