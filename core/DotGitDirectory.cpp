#include "DotGitDirectory.h"

namespace
{
    const std::string git(".git");
}

namespace YAM
{
    std::filesystem::path DotGitDirectory::find(std::filesystem::path const& directory) {
        std::filesystem::path repoDir(directory);
        while (!repoDir.empty() && !std::filesystem::exists(repoDir / git)) {
            auto parent = repoDir.parent_path();
            repoDir = (parent == repoDir) ? "" : parent;
        }
        std::filesystem::path gitDir;
        if (!repoDir.empty()) {
            gitDir = repoDir / git;
        }
        return gitDir;
    }

    DotGitDirectory::DotGitDirectory() : DotGitDirectory(std::filesystem::current_path())
    {}

    DotGitDirectory::DotGitDirectory(std::filesystem::path const& directory)
        : _dotGitDir(find(directory))
    {}
}
