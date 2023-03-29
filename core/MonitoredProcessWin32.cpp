#include "MonitoredProcessWin32.h"
#include "MSBuildTrackerOutputReader.h"
#include "FileSystem.h"

#include <iostream>
#include <chrono>
#include <limits>

namespace
{

    std::string trackerExe = R"("C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\arm64\Tracker.exe")";

    std::filesystem::path trackerLogDir(std::filesystem::path const& tmpDir) {
        return tmpDir / "trackerLogDir";
    }

    std::string generateCmd(
        std::string const& program, 
        std::string const& arguments,
        std::filesystem::path const& tmpDir
    ) {
        std::stringstream ss;
        ss
            << trackerExe << " /I " << trackerLogDir(tmpDir).string() << " /c "
            << program << " " << arguments; 
        auto cmd = ss.str();
        return cmd;
    }

    boost::process::environment generateEnv(
        std::filesystem::path const& tmpDir, 
        std::map<std::string, std::string> const& env
    ) {
        boost::process::environment thisenv = boost::this_process::environment();
        boost::process::environment bpenv;
        bpenv["SystemRoot"] = thisenv.at("SystemRoot").to_string();
        bpenv["TMP"] = tmpDir.string();
        for (auto const& pair : env) {
            bpenv[pair.first] = pair.second;
        }
        return bpenv;
    }
}

namespace YAM
{
    MonitoredProcessWin32::MonitoredProcessWin32(
        std::string const& program,
        std::string const& arguments,
        std::map<std::string, std::string> const& env)
        : IMonitoredProcess(program, arguments, env)
        , _tempDir(FileSystem::createUniqueDirectory().string())
        , _groupExited(false)
        , _childExited(false)
        , _child(
            generateCmd(_program, _arguments, _tempDir),
            generateEnv(_tempDir, _env),
            _group,
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
            MSBuildTrackerOutputReader reader(trackerLogDir(_tempDir));
            _result.readFiles = reader.readFiles();
            _result.writtenFiles = reader.writtenFiles();
            _result.readOnlyFiles = reader.readOnlyFiles();
            std::filesystem::remove_all(_tempDir);
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