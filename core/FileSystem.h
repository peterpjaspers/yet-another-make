#pragma once

#include <string>
#include <filesystem>

namespace YAM
{
	// Various functions to hide OS details.
	class __declspec(dllexport) FileSystem
	{
	public:
		// Create a directory in the system temporary folder.
		// Return normalized absolute path of created directory.
		static std::filesystem::path createUniqueDirectory();

		// Return a unique path in the system temporary folder.
		// Note: the path is not a normalized path.
		static std::filesystem::path uniquePath();

		// Linux and MacOs: return path unchanged.
		// Windows: 
		// if path is in long format: 
		//     return path in lowercase
		// if path is in short format and std::filesystem::exists(path):
		//     return path in long format in lowercase.
		// if path is in short format and !std::filesystem::exists(path):
		//     return path in short format in lowercase.
		//
		static std::filesystem::path normalizePath(std::wstring const& path);
		static std::filesystem::path normalizePath(std::filesystem::path const& path);
	};
}

