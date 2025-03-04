#pragma once

#include <string>
#include <chrono>
#include <map>
#include <set>
#include <filesystem>

namespace YAM
{
    struct __declspec(dllexport) MonitoredProcessResult
    {
        int exitCode;
        std::string stdOut;
        std::string stdErr;
        std::set<std::filesystem::path> readFiles;     // read-accessed files
        std::set<std::filesystem::path> writtenFiles;  // write-accessed files
        std::set<std::filesystem::path> readOnlyFiles; // readFiles except writtenFiles
        // last-write-times for readOnly and written files.
        // for readOnlyFiles: the last-write-time of the file at first read-access
        // for writtenFiles: the last-write-time of the file at last write-access
        // The map is empty when not supported by the implementation.
        std::map<std::filesystem::path, std::chrono::utc_clock::time_point> lastWriteTimes;

        void toLines(std::string const& str, std::vector<std::string>& lines) {
            auto ss = std::stringstream(str);
            for (std::string line; std::getline(ss, line, '\n');) {
                lines.push_back(line);
            }
        }
    };

    // Interface to start a process and to monitor it and its child  processes
    // (recursively) for file access.
    // 
    class __declspec(dllexport) IMonitoredProcess
    {
    public:
        // Start execution of 'program' using 'env' as environment and passing
        // 'arguments' to program.
        IMonitoredProcess(
            std::string const& program,
            std::string const& arguments,
            std::filesystem::path const& workingDir,
            std::map<std::string, std::string> const & env)
            : _program(program)
            , _arguments(arguments)
            , _workingDir(workingDir)
            , _env(env)
        {}

        // Wait for the process to complete.
        virtual MonitoredProcessResult const& wait() = 0;

        // Wait for the process to complete or for 'timoutInSeconds' to expire.
        // Return whether process exited while waiting (i.e. return false on
        // timeout).
        virtual bool wait_for(unsigned int timoutInMilliSeconds) = 0;

        // Terminate (kill) the process tree.
        // Example:
        //    if (!process.wait_for(std::chrono::seconds(10)) process.terminate();
        //    auto result = process.wait();
        virtual void terminate() = 0;

    protected:
        std::string _program;
        std::string _arguments;
        std::filesystem::path _workingDir;
        std::map<std::string, std::string> _env;
    };
}