#include "MonitoredProcessWin32.h"
#include "MSBuildTrackerOutputReader.h"
#include "FileSystem.h"
#include "Glob.h"
#include "../accessMonitor/Monitor.h"
#include <iostream>
#include <chrono>
#include <limits>
#include <locale>
#include <codecvt>
#include <string>

namespace
{
    std::string trackerExe = R"("D:\Programs\Microsoft Visual Studio\2022\community\MSBuild\Current\Bin\amd64\Tracker.exe")";

    std::wstring widen(std::string const& narrow) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wide = converter.from_bytes(narrow);
        return wide;
    }

    std::filesystem::path trackerLogDir(std::filesystem::path const& tmpDir) {
        return tmpDir / "trackerLogDir";
    }

    std::string generateCmd(
        std::string const& program, 
        std::string const& arguments
    ) {
        std::stringstream ss;
        ss << program << " " << arguments;
        return ss.str();
    }

    boost::process::environment generateEnv(
        std::filesystem::path const& tmpDir, 
        std::map<std::string, std::string> const& env
    ) {
        //boost::process::environment thisenv = boost::this_process::environment();
        boost::process::environment bpenv;
        //bpenv["SystemRoot"] = thisenv.at("SystemRoot").to_string();
        bpenv["TMP"] = tmpDir.string();
        bpenv["TEMP"] = tmpDir.string();
        for (auto const& pair : env) {
            bpenv[pair.first] = pair.second;
        }
        return bpenv;
    }

    std::filesystem::path getTempDirAndStartMonitoring(
        std::map<std::string, std::string> const& env
    ) {
        static std::string tmp("TMP");
        static std::string temp("TEMP");
        std::filesystem::path tempDir;

        if (env.count(tmp) > 0) tempDir = env.at(tmp);
        else if (env.count(temp) > 0) tempDir = env.at(temp);
        else tempDir = std::filesystem::canonical(
            std::filesystem::temp_directory_path());
        AccessMonitor::startMonitoring(tempDir);
        return tempDir; 
    }

    bool isSubpath(const std::filesystem::path& path, const std::filesystem::path& base) {
        const auto mismatch_pair = std::mismatch(path.begin(), path.end(), base.begin(), base.end());
        return mismatch_pair.second == base.end();
    }
}

namespace YAM
{ 
    MonitoredProcessWin32::MonitoredProcessWin32(
        std::string const& program,
        std::string const& arguments, 
        std::filesystem::path const& workingDir,
        std::map<std::string, std::string> const& env)
        : IMonitoredProcess(program, arguments, workingDir, env)
        , _tempDir(getTempDirAndStartMonitoring(env))
        , _groupExited(false)
        , _childExited(false)
        , _child(
            generateCmd(_program, _arguments),
            generateEnv(_tempDir, _env),
            _group,
            boost::process::start_dir(_workingDir.wstring()),
            boost::process::std_out > _stdout, 
            boost::process::std_err > _stderr,
            _ios)
    {
    }

    MonitoredProcessResult const& MonitoredProcessWin32::wait() {
        _ios.run();
        if (!_groupExited) {
            _group.wait();
            _groupExited = true;
        }
        if (!_childExited) {
            _child.wait();
            _childExited = true;
            _result.exitCode = _child.exit_code();
            _result.stdOut = _stdout.get();
            _result.stdErr = _stderr.get();
            AccessMonitor::MonitorEvents mfiles;
            AccessMonitor::stopMonitoring(&mfiles);
            for (auto const& pair : mfiles) {
                AccessMonitor::FileAccess const& fa(pair.second);
                std::filesystem::path filePath;
                if (pair.first.starts_with(L"//?")) {
                    filePath = pair.first.substr(3);
                } else {
                    filePath = pair.first;
                }
                if (
                    !Glob::isGlob(filePath) &&
                    !isSubpath(filePath, _tempDir) &&
                    std::filesystem::is_regular_file(filePath)
                ) {
                    if (fa.modes & AccessMonitor::AccessDelete) {
                        // ignore
                    }
                    if (fa.modes & AccessMonitor::AccessNone) {
                        _result.readFiles.insert(filePath);
                    }
                    if (fa.modes & AccessMonitor::AccessRead) {
                        _result.readFiles.insert(filePath);
                    }
                    if (fa.modes & AccessMonitor::AccessWrite) {
                        _result.writtenFiles.insert(filePath);
                    }
                }
                auto utcFileTime = decltype(fa.lastWriteTime)::clock::to_utc(fa.lastWriteTime);
                _result.lastWriteTimes.insert({ filePath, utcFileTime });
            }
            std::set_difference(
                _result.readFiles.begin(), _result.readFiles.end(),
                _result.writtenFiles.begin(), _result.writtenFiles.end(),
                std::inserter(_result.readOnlyFiles, _result.readOnlyFiles.begin()));
        }
        
        return _result;
    }

    bool MonitoredProcessWin32::wait_for(unsigned int timoutInMilliSeconds) {
        if (!_groupExited) {
            auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(timoutInMilliSeconds);
            _ios.run_until(deadline);
            _groupExited = _group.wait_until(deadline);
        }
        return _groupExited;
    }

    void MonitoredProcessWin32::terminate() {
        _group.terminate();
    }
}