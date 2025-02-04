#include "../MonitoredProcessWin32.h"
#include "../FileSystem.h"

#include "gtest/gtest.h"
#include <chrono>
#include <string>
#include <vector>
#include <thread>
#include <boost/process.hpp>
#include <fstream>

#include "../../accessMonitor/Monitor.h"

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
        AccessMonitor::enableMonitoring();
        MonitoredProcessWin32 ping(pingExe, "-n 3 127.0.0.1", wdir, env);
        //ping will take approximately n (==3) seconds. Use larger timeout.
        EXPECT_TRUE(ping.wait_for(15000));
        MonitoredProcessResult result = ping.wait();
        ASSERT_EQ(0, result.exitCode);
        AccessMonitor::disableMonitoring();

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
        AccessMonitor::enableMonitoring();
        MonitoredProcessWin32 cmd(cmdExe, "/c dir notlikelytoexist.blabla", wdir, env);
        EXPECT_TRUE(cmd.wait_for(15000));
        MonitoredProcessResult result = cmd.wait();
        ASSERT_EQ(1, result.exitCode);
        AccessMonitor::disableMonitoring();
        std::string expected("File Not Found");
        EXPECT_EQ(std::string::npos, result.stdOut.find(expected));
        EXPECT_EQ(0, result.stdErr.find(expected));
    }

    TEST(MonitoredProcessWin32, passEnvironment) {
        std::map<std::string, std::string> env;
        env["rubbish"] = "nonsense";

        auto cmdExe = boost::process::search_path("cmd").string();
        AccessMonitor::enableMonitoring();
        MonitoredProcessWin32 cmd(cmdExe, "/c echo %rubbish% > c:\\temp\\junk.txt & type c:\\temp\\junk.txt", wdir, env);
        EXPECT_TRUE(cmd.wait_for(15000));
        MonitoredProcessResult result = cmd.wait();
        ASSERT_EQ(0, result.exitCode);
        AccessMonitor::disableMonitoring();
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
        AccessMonitor::enableMonitoring();
        MonitoredProcessWin32 ping(pingExe, "-n 3000 127.0.0.1", wdir, env);
        std::thread t(terminate, std::ref(ping));
        MonitoredProcessResult result = ping.wait();
        ASSERT_EQ(1, result.exitCode);
        AccessMonitor::disableMonitoring();
        t.join();
    }

   TEST(MonitoredProcessWin32, fileDependencies) {
        WorkingDir tempDir;
        std::string cmdExeStr = boost::process::search_path("cmd").string();
        std::filesystem::path cmdExe = std::filesystem::canonical(cmdExeStr);
        std::map<std::string, std::string> env;

        AccessMonitor::enableMonitoring();
        MonitoredProcessWin32 cmd(
            cmdExe.string(),
            " /c echo rubbish > junk.txt & type junk.txt",
            wdir,
            env);
        EXPECT_TRUE(cmd.wait_for(15000));
        MonitoredProcessResult result = cmd.wait();
        ASSERT_EQ(0, result.exitCode);
        AccessMonitor::disableMonitoring();

        EXPECT_EQ(2, result.readFiles.size());
        EXPECT_EQ(1, result.readOnlyFiles.size());
        EXPECT_EQ(1, result.writtenFiles.size());
        EXPECT_TRUE(result.readFiles.contains(cmdExe));
        EXPECT_TRUE(result.readOnlyFiles.contains(cmdExe));
        EXPECT_TRUE(result.readFiles.contains(wdir / "junk.txt"));
        EXPECT_TRUE(result.writtenFiles.contains(wdir / "junk.txt"));
    }
}
