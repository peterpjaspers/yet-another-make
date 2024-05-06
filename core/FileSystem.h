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
        // Postfix the directory path with the given postfix. 
        static std::filesystem::path createUniqueDirectory(std::string const& postfix = "");

        // Return a unique path in the system temporary folder.
        // Postfix the directory path with the given postfix. 
        // Note: the path is not a normalized path.
        static std::filesystem::path uniquePath(std::string const& postfix = "");

        // Linux and MacOs: 
        //     return std::filesystem::canonical(path) or, if path does not
        //     exist return path itself.
        // Windows: 
        //     return lower-cased std::filesystem::canonical(path) or, if path 
        //     does not  exist return lowercased path.
        //
        static std::filesystem::path canonicalPath(std::filesystem::path const& path);
    };
}

