#define _CRT_SECURE_NO_WARNINGS

#include "../FileSystem.h"
#include "../Glob.h"
#include "../MSBuildTrackerOutputReader.h"

#include "gtest/gtest.h"
#include <string>
#include <boost/process.hpp>
#include <fstream>

#include "../../accessMonitor/Monitor.h"

namespace
{
    using namespace YAM;

    bool isSubpath(const std::filesystem::path& path, const std::filesystem::path& base) {
        const auto mismatch_pair = std::mismatch(path.begin(), path.end(), base.begin(), base.end());
        return mismatch_pair.second == base.end();
    }

    class WorkingDir {
    public:
        std::filesystem::path dir;
        WorkingDir() : dir(FileSystem::createUniqueDirectory()) {}
        ~WorkingDir() { std::filesystem::remove_all(dir); }
    };

    std::filesystem::path wdir(std::filesystem::current_path());

    TEST(AccessMonitor, compareWithMSBuildTracker) {
        WorkingDir tempDir;
        std::string cmdExe = boost::process::search_path("cmd").string();
        boost::process::environment env;
        env["TMP"] = tempDir.dir.string();
        env["TEMP"] = tempDir.dir.string();
        std::filesystem::remove_all("generated");
        std::string unzipCmd(R"("C:\Program Files\7-Zip\7z.exe" e -y -ogenerated\rawImages rawImages.7z)");

        AccessMonitor::enableMonitoring();
        AccessMonitor::startMonitoring(tempDir.dir.string());
        {
            boost::process::child unzip(unzipCmd, env);
            unzip.wait();
            auto exitCode = unzip.exit_code();
            EXPECT_EQ(0, exitCode);
        }
        AccessMonitor::MonitorEvents unfilteredResult;
        AccessMonitor::stopMonitoring(&unfilteredResult);
        AccessMonitor::disableMonitoring();
        AccessMonitor::MonitorEvents result;
        for (auto& pair : unfilteredResult) {
            std::filesystem::path filePath;
            if (pair.first.string().starts_with("//?")) {
                filePath = pair.first.string().substr(3);
            } else {
                filePath = pair.first;
            }
            if (
                !Glob::isGlob(filePath) &&
                !isSubpath(filePath.make_preferred(), tempDir.dir) &&
                std::filesystem::is_regular_file(filePath)
            ) {
                result.insert({ filePath.generic_wstring(), pair.second});
            }
        }
        std::filesystem::remove_all("generated");
        // MSBuild tracker does not report read-access on these files:
        result.erase(std::wstring(L"C:/Program Files/7-Zip/7z.dll"));
        result.erase(std::wstring(L"C:/Program Files/7-Zip/7z.exe"));


        std::string trackerExe = R"("D:\Programs\Microsoft Visual Studio\2022\community\MSBuild\Current\Bin\amd64\Tracker.exe")";
        std::filesystem::path trackerLogDir(tempDir.dir / "trackerLogDir");
        std::string trackerCmd = trackerExe + " /I " + trackerLogDir.string() + " /c " + unzipCmd;
        boost::process::child tracker(trackerCmd, env);
        tracker.wait();
        auto exitCode = tracker.exit_code();
        EXPECT_EQ(0, exitCode);
        MSBuildTrackerOutputReader reader(trackerLogDir);
        auto readFiles = reader.readFiles();
        auto writtenFiles = reader.writtenFiles();
        auto readOnlyFiles = reader.readOnlyFiles();

        for (auto const& file : readFiles) {
            ASSERT_TRUE(result.contains(file.generic_wstring()));
            auto fileAccess = result[file.generic_wstring()];
            EXPECT_EQ(AccessMonitor::AccessRead, fileAccess.modes() & AccessMonitor::AccessRead);
        }
        for (auto const& file : writtenFiles) {
            ASSERT_TRUE(result.contains(file.generic_wstring()));
            auto fileAccess = result[file.generic_wstring()];
            EXPECT_EQ(AccessMonitor::AccessWrite, fileAccess.modes() & AccessMonitor::AccessWrite);
        }
        for (auto const& file : readOnlyFiles) {
            ASSERT_TRUE(result.contains(file.generic_wstring()));
            auto fileAccess = result[file.generic_wstring()];
            EXPECT_EQ(0, fileAccess.modes() & AccessMonitor::AccessWrite);
        }

        for (auto const& pair : result) {
            std::filesystem::path file(pair.first);
            if (pair.second.modes() & AccessMonitor::AccessRead) {
                if (pair.second.modes() & AccessMonitor::AccessWrite) {
                    if (!writtenFiles.contains(file.make_preferred())) {
                        EXPECT_TRUE(false);
                        std::cout << "writtenFiles does not contain read+written " << file.make_preferred();
                    }
                    if (readOnlyFiles.contains(file.make_preferred())) {
                        EXPECT_TRUE(false);
                        std::cout << "readOnlyFiles contains read+written " << file.make_preferred();
                    }
                } else {
                    if (!readFiles.contains(file.make_preferred())) {
                        EXPECT_TRUE(false);
                        std::cout << "readFiles does not contain read-only " << file.make_preferred();
                    }
                    if (!readOnlyFiles.contains(file.make_preferred())) {
                        EXPECT_TRUE(false);
                        std::cout << "readOnlyFiles does not contain read-only " << file.make_preferred();
                    }
                }
            } else if (pair.second.modes() & AccessMonitor::AccessWrite) {
                if (!writtenFiles.contains(file.make_preferred())) {
                    EXPECT_TRUE(false);
                    std::cout << "writtenFiles does not contain written " << file.make_preferred();
                }
                if (readFiles.contains(file.make_preferred())) {
                    EXPECT_TRUE(false);
                    std::cout << "readFiles does contains written " << file.make_preferred();
                }
                if (readOnlyFiles.contains(file.make_preferred())) {
                    EXPECT_TRUE(false);
                    std::cout << "readOnlyFiles does contains written " << file.make_preferred();
                }
            } else {
                EXPECT_TRUE(false);
            }
        }
    }

    void writeFile(std::filesystem::path p, std::string const& content) {
        std::ofstream stream(p.string().c_str());
        EXPECT_TRUE(stream.is_open());
        stream << content;
        stream.close();
    }

    TEST(AccessMonitor, CreateProcessW) {
        std::filesystem::remove("junk.txt");

        WorkingDir tempDir;
        std::wstring cmdExe = boost::process::search_path("cmd.exe").wstring();
        std::wstring cmd = cmdExe + L" /c echo %TMP% > junk.txt & type junk.txt";

        STARTUPINFO si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = 0x00000100;
        si.hStdInput = INVALID_HANDLE_VALUE;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        boost::process::environment bpenv;
        bpenv["TMP"] = tempDir.dir.string();
        bpenv["TEMP"] = tempDir.dir.string();
        const char* env = bpenv.native_handle();

        BOOL inheritHandles = TRUE;
        DWORD creationFlags = 0; // CREATE_UNICODE_ENVIRONMENT;
        AccessMonitor::enableMonitoring();
        AccessMonitor::startMonitoring(tempDir.dir);
        if (!CreateProcessW(
            cmdExe.c_str(), // lpApplicationName
            cmd.data(),     // lpCommandLine
            NULL,           // lpProcessAttributes
            NULL,           // lpThreadAttributes
            inheritHandles, // bInheritHandles
            creationFlags,  // dwCreationFlags
            reinterpret_cast<void*>(const_cast<char*>(env)), // lpEnvironment
            NULL,           // lpCurrentDirectory 
            &si,            // lpStartupInfo
            &pi)            // lpProcessInformation
        ) {
            printf("CreateProcess failed (%d).\n", GetLastError());
            return;
        }

        // Wait until child process exits.
        WaitForSingleObject(pi.hProcess, INFINITE);
        AccessMonitor::MonitorEvents result;
        AccessMonitor::stopMonitoring(&result);
        AccessMonitor::disableMonitoring();
        std::wstring junkPath = std::filesystem::path(wdir / "junk.txt").generic_wstring();
        ASSERT_TRUE(result.contains(junkPath));
        auto fileAccess = result.at(junkPath);
        EXPECT_EQ(AccessMonitor::AccessNone | AccessMonitor::AccessWrite, fileAccess.mode());
        EXPECT_EQ(AccessMonitor::AccessNone | AccessMonitor::AccessRead | AccessMonitor::AccessWrite, fileAccess.modes());

        // Close process and thread handles. 
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    TEST(AccessMonitor, remoteTest) {
        std::filesystem::path remoteSessionDir(std::filesystem::temp_directory_path() / "RemoteSession");
        std::error_code ec;
        std::filesystem::remove(remoteSessionDir, ec);

        auto remoteTest = boost::process::search_path("remoteTest.exe").string();
        WorkingDir tempDir;
        AccessMonitor::enableMonitoring();
        AccessMonitor::startMonitoring(tempDir.dir.string());
        int exitCode = system(remoteTest.c_str());
        AccessMonitor::MonitorEvents result;
        AccessMonitor::stopMonitoring(&result);
        AccessMonitor::disableMonitoring();
        EXPECT_GE(result.size(), 21);
    }
}