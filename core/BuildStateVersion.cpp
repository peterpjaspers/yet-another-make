#include "BuildStateVersion.h"
#include "ILogBook.h"

#include <regex>

namespace
{
    uint32_t _writeVersion = 1;
    std::vector<uint32_t> _readableVersions = { _writeVersion };
    const std::string _prefix("buildstate_");
    const std::string _ext("bt");
    const std::regex _nameRe("^" + _prefix + "([0-9]+)\\." + _ext + "$");
    const uint32_t _invalid = 0;

    bool isReadableVersion(uint32_t version) {
        auto it = std::find(_readableVersions.begin(), _readableVersions.end(), version);
        return it != _readableVersions.end();
    }

    std::filesystem::path buildStatePath(
        std::filesystem::path const& buildStateDir,
        uint32_t version
    ) {
        std::stringstream ss;
        ss << _prefix << version << "." << _ext;
        return buildStateDir / ss.str();
    }

    // Scan buildStateDir for existance of buildstate files that match _nameRe.
    // Return buildstate file path and its version. In case multiple files
    // are found return file with highest version.
    // Return empty path if no buildstate file was found.
    std::pair<std::filesystem::path, uint32_t> findBuildStateFile(std::filesystem::path const& buildStateDir) {
        uint32_t fileVersion = 0;
        std::filesystem::path buildStatePath;
        for (auto const& entry : std::filesystem::directory_iterator(buildStateDir)) {
            auto fileName = entry.path().filename().string();
            std::cmatch cm;
            bool matched = std::regex_search(fileName.c_str(), cm, _nameRe);
            if (matched) {
                std::string number = cm[1].str();
                std::stringstream ss(number);
                uint32_t version;
                ss >> version;
                if (version >= fileVersion) {
                    fileVersion = version;
                    buildStatePath = entry.path();
                }
            }
        }
        return { buildStatePath, fileVersion };
    }
}

namespace YAM
{
    void BuildStateVersion::writeVersion(uint32_t version) {
        if (version == _invalid) throw std::runtime_error("invalid version");
        _writeVersion = version;
    }
    void BuildStateVersion::readableVersions(std::vector<uint32_t> const& versions) {
        for (auto v : versions) {
            if (v == _invalid) throw std::runtime_error("invalid version");
            if (v > _writeVersion) throw std::runtime_error("invalid version");
        }
        _readableVersions = versions;
    }
    uint32_t BuildStateVersion::writeVersion() {
        return _writeVersion;
    }
    std::vector<uint32_t> const& BuildStateVersion::readableVersions() {
        return _readableVersions;
    }

    std::filesystem::path BuildStateVersion::select(
        std::filesystem::path const& buildStateDir,
        ILogBook& logBook
    ) {
        std::filesystem::create_directories(buildStateDir);
        auto [buildFile, version] = findBuildStateFile(buildStateDir);
        if (buildFile.empty()) {
            buildFile = buildStatePath(buildStateDir, _writeVersion);
        } else if (!isReadableVersion(version)) {
            auto oldBuildFile = buildFile;
            buildFile = "";
            std::stringstream ss;
            ss << "Buildstate file " << oldBuildFile.string() << " has an incompatible version." << std::endl;
            ss << "This renders all previously generated build outputs stale." << std::endl;
            ss << "If you want to build this repository you must delete the buildstate file," << std::endl;
            ss << "delete all previously generated build output files and then restart the build." << std::endl;
            LogRecord error(LogRecord::Aspect::Error, ss.str());
            logBook.add(error);
        } else if (version != _writeVersion) {
            auto oldBuildFile = buildFile;
            std::stringstream ss;
            buildFile = buildStatePath(buildStateDir, _writeVersion);
            std::filesystem::copy_file(oldBuildFile, buildFile);
            ss << "Buildstate file " << oldBuildFile.string() << " has an old, but compatible, version." << std::endl;
            ss << "The file is upgraded to " << buildFile.string() << std::endl;
            LogRecord progress(LogRecord::Aspect::Progress, ss.str());
            logBook.add(progress);
        }
        return buildFile;
    }
}
