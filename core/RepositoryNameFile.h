#pragma once

#include <filesystem>
#include <string>
#include <filesystem>

namespace YAM
{
    // Class providing access to repoDir/yamConfig/repoName.txt
    // This file contains then name of the repository.
    //
    class __declspec(dllexport) RepositoryNameFile
    {
    public:
        RepositoryNameFile(std::filesystem::path const& repoDir);

        // Set and write repoName to file
        void repoName(std::string const& repoName);

        // Read repo name from file.
        std::string const repoName() const;

    private:
        std::filesystem::path _repoDir;
        std::string _repoName;
    };

    class __declspec(dllexport) RepositoryNamePrompt
    {
    public:
        std::string operator()(std::filesystem::path const& repoDir) const;
    };
}


