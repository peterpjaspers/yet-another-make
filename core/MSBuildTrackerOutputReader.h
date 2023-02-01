#pragma once

#include <filesystem>
#include <set>

namespace YAM
{
	// Read the log files produced by the Microsoft Build file dependency
	// tracker Tracker.exe.
	// 
	class __declspec(dllexport) MSBuildTrackerOutputReader
	{
	public:
		// Read the Tracker.exe logfiles from the given directory.
		MSBuildTrackerOutputReader(std::filesystem::path const& logDir);

		// Return the dependencies read from the Tracker.exe logfiles
		// readFiles contains the files that were read-accessed.
		// writtenFiles contains the files that were write-accessed.
		// readOnlyFiles = readFiles except writtenFiles
		// All paths are absolute.
		std::set<std::filesystem::path> const& readFiles() const;
		std::set<std::filesystem::path> const& writtenFiles() const;
		std::set<std::filesystem::path> const& readOnlyFiles() const;

	private:
		void parseDependencies(
			std::filesystem::path const& logFile,
			std::set<std::filesystem::path>& dependencies);

		std::set<std::filesystem::path> _readFiles;
		std::set<std::filesystem::path> _writtenFiles;
		std::set<std::filesystem::path> _readOnlyFiles;
	};
}


