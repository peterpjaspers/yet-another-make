#pragma once

#include <filesystem>

namespace YAM
{
    class __declspec(dllexport) DotYamDirectory
    {
    public:
        // If no directory/.yam directory is found: create it.
        // Return the absolute path of the .yam directory.
        static std::filesystem::path create(std::filesystem::path const& directory);

        // If directory/.yam directory exists: return that path.
        // Else return get(directory.parent_path());
        // Return empty path when not found.
        static std::filesystem::path find(std::filesystem::path const& directory);

        // Construct (find) .yam dir associated with current directory.
        DotYamDirectory();

        // Construct (find) .yam dir associated with given directory.
        DotYamDirectory(std::filesystem::path const& directory);

        std::filesystem::path const& dotYamDir() const { return _dotYamDir; }

    private:
        std::filesystem::path _dotYamDir;
    };
}


