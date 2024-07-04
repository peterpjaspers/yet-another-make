#include "RepositoryNameFile.h"
#include <iostream>
#include <fstream>
#include <regex>

namespace
{
    using namespace YAM;

    std::string readFile(std::filesystem::path const& path) {
        std::ifstream file(path);
        if (file.good()) {
            std::stringstream ss;
            ss << file.rdbuf();
            return ss.str();
        }
        return "";
    }

    void writeFile(std::filesystem::path p, std::string const& content) {
        std::ofstream stream(p.string().c_str());
        stream << content;
        stream.close();
    }

    std::filesystem::path repoNameFilePath(std::filesystem::path const& repoDir) {
        return repoDir / "yamConfig/repoName.txt";
    }

    std::string readRepoName(std::filesystem::path const& repoDir) {
        return readFile(repoNameFilePath(repoDir));
    }
    bool yes(std::string const& input) {
        return input == "y" || input == "Y";
    }

    bool no(std::string const& input) {
        return input == "n" || input == "N";
    }

    bool confirmRepoDir(std::filesystem::path const& repoDir) {
        std::cout << "Initializing yam on directory " << repoDir << std::endl;
        std::cout << "Make sure that this is the root directory of your source code repository.";
        std::cout << std::endl;
        std::cout << "If this is not the case then restart yam on the proper directory.";
        std::cout << std::endl;
        std::string input;
        do {
            std::cout << "Please confirm using this directory [y|n]:";
            std::cin >> input;
        } while (!yes(input) && !no(input));
        return yes(input);
    }

    bool confirmRepoName(std::string const& repoName) {
        std::regex re(R"(^[\w0123456789_\-]*$)");
        if (!std::regex_match(repoName, re)) {
            std::cout << "Invalid repository name: valid chars are a-z, A-Z, 0-9, _, -";
            std::cout << std::endl;
            return false;
        }
        std::cout << "Yam will use the following repository name: " << repoName << std::endl;
        std::string input;
        do {
            std::cout << "Please confirm using this name [y|n]: ";
            std::cin >> input;
        } while (!yes(input) && !no(input));
        return yes(input);
    }

    std::string promptRepoName(std::filesystem::path const& repoDir) {
        std::string repoName;
        if (confirmRepoDir(repoDir)) {
            std::string input;
            do {
                std::cout << "Enter the name of the repository: ";
                std::cin >> input;
            } while (!confirmRepoName(input));
            repoName = input;
        } else {
            std::cout << "Restart yam at the root directory of your source code repository" << std::endl;
        }
        return repoName;
    }

}

namespace YAM
{
    RepositoryNameFile::RepositoryNameFile(std::filesystem::path const& repoDir)
        : _repoDir(repoDir)
        , _repoName(readRepoName(repoDir))
    {}

    void RepositoryNameFile::repoName(std::string const& repoName) {
        std::filesystem::create_directories(_repoDir / "yamConfig");
        writeFile(repoNameFilePath(_repoDir), repoName);
        _repoName = readRepoName(_repoDir);
    }

    std::string const RepositoryNameFile::repoName() const {
        return _repoName;
    }

    std::string RepositoryNamePrompt::operator()(std::filesystem::path const& repoDir) const {
        return promptRepoName(repoDir);
    }
}