#pragma once

#include <string>
#include <filesystem>

namespace YAM
{
    // Various functions to hide OS details.
    class __declspec(dllexport) FileSystem
    {
    public:
        static std::filesystem::path yamTempFolder();

        // If not yet exists: create directory yam_temp folder in the system temporary
        // folder. Create a directory in the yam_temp folder .
        // Return normalized absolute path of created directory.
        // Prefix the directory path with the given prefix. 
        static std::filesystem::path createUniqueDirectory(std::string const& prefix = "");

        // Return a unique path in the yam_temp folder.
        // Prefix the directory path with the given prefix. 
        // Note: the path is not a normalized path.
        static std::filesystem::path uniquePath(std::string const& prefix = "");

        // Return std::filesystem::canonical(path) or, if path does not
        // exist return path itself.
        static std::filesystem::path canonicalPath(std::filesystem::path const& path);

        // Return lower-case version of path.
        // This implementation only works for plain ascii paths.
        // Converting a path to lower-case is non-trivial.See 
        // https://stackoverflow.com/questions/313970/how-to-convert-an-instance-of-stdstring-to-lower-case?rq=2
        static std::filesystem::path toLower(std::filesystem::path const& path);
    };

    // Creates a directory and deletes it when the object goes 
    // out-of-scope
    //
    class __declspec(dllexport) TemporaryDirectory {
    public:
        std::filesystem::path dir;
        TemporaryDirectory() : dir(FileSystem::createUniqueDirectory()) {}
        TemporaryDirectory(std::string prefix) : dir(FileSystem::createUniqueDirectory(prefix)) {}
        ~TemporaryDirectory() { std::filesystem::remove_all(dir); }
    };
}

