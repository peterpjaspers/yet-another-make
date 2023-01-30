#pragma once

#include <string>
#include <filesystem>

namespace YAM
{
	// Various functions to hide OS details.
	class __declspec(dllexport) FileSystem
	{
	public:
		// Create in the system temporary folder a sub-directory.
		// Return normalized absolute path of created directory.
		static std::filesystem::path createUniqueDirectory();

		// Create in the system temporary folder a file.
		// Return normalized absolute path of created file.
		static std::filesystem::path createUniqueFile();

		// Create a unique path in the system temporary folder.
		// Return the path.
		// Note: the path has not been converted to normalized path
		// name because this function does not create the file.
		static std::filesystem::path uniquePath();

		// The normalized path functions hide the Windows specific feature
		// that directories/files can be accessed with path names that 
		// differ in format (short, long) and casing.
		//   
		// 'path' identifies an existing file or directory. Return path in
		// normalized format.Conversion is only possible for paths that 
		// reference an existing file or directory.
		static std::filesystem::path normalizePath(std::wstring const& path);
		static std::filesystem::path normalizePath(std::filesystem::path const& path);
	};
}

