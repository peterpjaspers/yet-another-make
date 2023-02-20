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

		// Linux and MacOs: 
		//     return std::filesystem::canonical(path) or, if path does not
		//     exist return path itself.
		// Windows: 
		//     return lower-cased std::filesystem::canonical(path) or, if path 
		//     does not  exist return lowercased path.
		//
		static std::filesystem::path normalizePath(std::filesystem::path const& path);
	};
}

