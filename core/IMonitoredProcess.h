#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <map>

namespace YAM
{
	struct __declspec(dllexport) MonitoredProcessResult
	{
		int exitCode;
		std::string stdOut;
		std::string stdErr;
		std::vector<std::filesystem::path> readFiles;     // read-accessed files
		std::vector<std::filesystem::path> writtenFiles;  // write-accessed files
		std::vector<std::filesystem::path> readOnlyFiles; // read- and not write-accessed files

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
			std::map<std::string, std::string> const & env)
			: _program(program)
			, _arguments(arguments)
			, _env(env)
		{}

		// Wait for the script to complete.
		virtual MonitoredProcessResult const& wait() = 0;

		// Wait for the script to complete or for 'timoutInSeconds' to expire.
		// Return whether script exited while waiting (i.e. return false on
		// timeout).
		virtual bool wait_for(unsigned int timoutInMilliSeconds) = 0;

		// Terminate (kill) the process tree.
		// Example:
		//     if (!executor.wait_for(std::chrono::seconds(10)) g.terminate();
		//	   auto result = executor.wait();
		virtual void terminate() = 0;

	protected:
		std::string _program;
		std::string _arguments;
		std::map<std::string, std::string> _env;
	};
}