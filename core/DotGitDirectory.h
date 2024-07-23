#pragma once

#include <filesystem>

namespace YAM
{
    class __declspec(dllexport) DotGitDirectory
    {
    public:
        // If directory/.git directory exists: return that path.
        // Else return find(directory.parent_path());
        // Return empty path when not found.
        static std::filesystem::path find(std::filesystem::path const& directory);

        // Construct (find) .git dir associated with current directory.
        DotGitDirectory();

        // Construct (find) .git dir associated with given directory.
        DotGitDirectory(std::filesystem::path const& directory);

        std::filesystem::path const& dotGitDir() const { return _dotGitDir; }

    private:
        std::filesystem::path _dotGitDir;
    };
}


