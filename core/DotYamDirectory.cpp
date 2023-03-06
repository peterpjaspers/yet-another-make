#include "DotYamDirectory.h"

namespace
{
    const std::string yam(".yam");
}

namespace YAM
{
    std::filesystem::path DotYamDirectory::create(std::filesystem::path const& directory) {
        std::filesystem::path yamDir(directory / yam);
        while (!std::filesystem::exists(yamDir)) {
            std::filesystem::create_directory(yamDir);
        }
        return yamDir;
    }

    std::filesystem::path DotYamDirectory::find(std::filesystem::path const& directory) {
        std::filesystem::path repoDir(directory);
        while (!repoDir.empty() && !std::filesystem::exists(repoDir / yam)) {
            auto parent = repoDir.parent_path();
            repoDir = (parent == repoDir) ? "" : parent;
        }
        std::filesystem::path yamDir;
        if (!repoDir.empty()) {
            yamDir = repoDir / yam;
            std::filesystem::create_directory(yamDir);
        }
        return yamDir;
    }

    DotYamDirectory::DotYamDirectory() : DotYamDirectory(std::filesystem::current_path())
    {}

    DotYamDirectory::DotYamDirectory(std::filesystem::path const& directory)
        : _dotYamDir(find(directory))
    {}
}
