#pragma once
#include "DotGitDirectory.h"

#include <filesystem>

namespace YAM
{
    class ILogBook;

    class __declspec(dllexport) DotYamDirectory
    {
    public:

        // Return .yam 
        static std::string const& yamName();

        // If 'directory' is (in) a git repository then create the .yam
        // directory in the root directory of the git repo, i.e. in the
        // directory that contains the .git directory. Fail if a .yam
        // directory already exists in one of the directories between
        // 'directory' and the git root dir.
        // Rationale: YAM FileRepositoryNode uses the .gitignore files 
        // and must therefore be able to monitor all .gitignore files for
        // changes. This can only be done when monitoring the git repo dir.
        // 
        // If 'directory' is not a git repo then find(directory). If not
        // found, then create(directory). Return found/created directory.
        // 
        // Return empty path on failure and log error in logBook.
        static std::filesystem::path initialize(
            std::filesystem::path const& directory,
            ILogBook* logBook);

        // If no directory/.yam directory is found: create it.
        // Return the absolute path of the .yam directory.
        static std::filesystem::path create(std::filesystem::path const& directory, ILogBook* logBook = nullptr);

        // If directory/.yam directory exists: return that path.
        // Else return find(directory.parent_path());
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


