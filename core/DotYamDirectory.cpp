#include "DotYamDirectory.h"
#include "ILogBook.h"

namespace
{
    const std::string yam(".yam");
}

namespace YAM
{
    std::string const& DotYamDirectory::yamName() {
        return yam;
    }

    std::filesystem::path DotYamDirectory::initialize(
        std::filesystem::path const& directory, 
        ILogBook* logBook) 
    {
        std::filesystem::path gitDir = DotGitDirectory::find(directory);
        std::filesystem::path yamDir = DotYamDirectory::find(directory);
        if (gitDir.empty() && yamDir.empty()) {
            yamDir = create(directory, logBook);
        } else if (!gitDir.empty() && yamDir.empty()) {
            yamDir = create(gitDir.parent_path(), logBook);
        } else if (gitDir.empty() && !yamDir.empty()) {
        } else if (!gitDir.empty() && !yamDir.empty()) {
            if (gitDir.parent_path() != yamDir.parent_path()) {
                std::stringstream ss;
                ss
                    << "YAM initialization failed" << std::endl
                    << "Reason:" << "a .yam directory already exists below the git root directory." << std::endl
                    << "    .yam dir: " << yamDir.string() << std::endl
                    << "    .git dir: " << gitDir.parent_path().string() << std::endl
                    << "Fix: delete " << yamDir.string() << " and retry initialization."
                    << std::endl;
                logBook->add(LogRecord(LogRecord::Error, ss.str()));
                yamDir.clear();
            } else {
                yamDir = create(gitDir.parent_path(), logBook);
            }
        }
        return yamDir;
    }

    std::filesystem::path DotYamDirectory::create(std::filesystem::path const& directory, ILogBook* logBook) {
        std::filesystem::path yamDir(directory / yam);
        if (!std::filesystem::exists(yamDir)) {
            std::filesystem::create_directory(yamDir);
            if (logBook != nullptr) {
                std::stringstream ss;
                ss << "YAM successfully initialized in directory " << yamDir.string() << std::endl;
                logBook->add(LogRecord(LogRecord::Progress, ss.str()));
            }
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
        }
        return yamDir;
    }

    DotYamDirectory::DotYamDirectory() : DotYamDirectory(std::filesystem::current_path())
    {}

    DotYamDirectory::DotYamDirectory(std::filesystem::path const& directory)
        : _dotYamDir(find(directory))
    {}
}
